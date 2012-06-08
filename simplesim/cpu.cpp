#include "cache.h"
#include "des.h"
#include "debug.h"
#include "cpu.h"

#include <stdint.h>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

using namespace SimpleSim;

RandomCpu::RandomCpu(unsigned id, double m, double p, unsigned mo) :
  name(std::string("RandomCpu") + toStr(id)),
  outstandingReqs(name + ":outstandingReqs", false),
  completedReqs(name + ":completedReqs"),
  probChangeBase(p), mtba(m), maxOutstandingReqs(mo)
{
  Slide::schedule(uniformRand(2*mtba), this, &RandomCpu::nextAccess, 
                  new addr_t((rand()%16)<<10));
}

void RandomCpu::nextAccess(SimpleSim::addr_t *a) {
  if (outstandingReqs < maxOutstandingReqs) {
    if (fRand() <= probChangeBase) {
      *a = (rand()%16) << 10;
    } else {
      *a += 8;
    }

    MemReq *mr = new MemReq(*a, rand()%2);
    mr->pushDev(this);
    lowerLevel->req(mr);

    Slide::schedule(uniformRand(2*mtba), this, &RandomCpu::nextAccess, a);
    ++outstandingReqs;
  } else {
    Slide::schedule(1, this, &RandomCpu::nextAccess, a);
  }
}

void RandomCpu::resp(MemReq *mr) {
  DBG(std::dec << Slide::_now << ": " << mr << ".\n");
  delete mr;
  --outstandingReqs;
  ++completedReqs;
}

template <typename CBObj> struct CallbackAdaptor {
  CallbackAdaptor(Qsim::OSDomain &osd): osd(osd), running(true) {
    osd.set_inst_cb(this, &CallbackAdaptor::inst_cb);
    osd.set_mem_cb(this, &CallbackAdaptor::mem_cb);
    osd.set_reg_cb(this, &CallbackAdaptor::reg_cb);
    osd.set_int_cb(this, &CallbackAdaptor::int_cb);
    osd.set_app_end_cb(this, &CallbackAdaptor::app_end_cb);
  }

  void addCpu(CBObj *c) { cpus.push_back(c); }

  bool finished() { return !running; }

  int app_end_cb(int c) {
    running = false;
    return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, \
               enum inst_type t)
  {
    cpus[c]->instCB(v, p, l, b, t);
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
    cpus[c]->memCB(v, p, s, w);
  }

  void reg_cb(int c, int r, uint8_t s, int w) {
    cpus[c]->regCB(r, s, w);
  }

  int int_cb(int c, uint8_t v) {
    cpus[c]->intCB(v);
    return 0;
  }

  bool running;
  std::vector<CBObj *> cpus;
  Qsim::OSDomain &osd;
};

// TODO: Put SimpleCpu and SuperscalarCpu in different files so each can have its
// own "cba" variable and the following hack can be eliminated:
static CallbackAdaptor<SimpleCpu> *cba_simplecpu(NULL);
#define cba cba_simplecpu

SimpleCpu::SimpleCpu(unsigned id, Qsim::OSDomain &osd):
  id(id),
  name(std::string("SimpleCpu") + toStr(id)),
  outstandingReqs(name + std::string(":outstandingReqs"), false),
  completedReqs(name + std::string(":completedReqs")),
  committedInsts(name + std::string(":committedInsts")),
  idleInsts(name + std::string(":idleInsts")),
  cpuRan(false)
{
  if (!cba) cba = new CallbackAdaptor<SimpleCpu>(osd);
  ASSERT(&osd == &cba->osd);

  cba->addCpu(this);

  Slide::schedule(0, this, &SimpleCpu::next, this);
  Slide::schedule(1000000, this, &SimpleCpu::timer, this);
}

void SimpleCpu::next(SimpleCpu *) {
  if (outstandingReqs == 0) {
    if (cpuRan) {
      ++committedInsts;
      if (cba->osd.idle(id)) ++idleInsts;
    }
    cpuRan = (cba->osd.run(id, 1) == 1);
  }
  if (!cba->finished()) Slide::schedule(1, this, &SimpleCpu::next, this);
  else {
    Slide::_terminate();
  }
}

void SimpleCpu::timer(SimpleCpu *) {
  cba->osd.timer_interrupt();
  Slide::schedule(1000000, this, &SimpleCpu::timer, this);
}

void SimpleCpu::resp(MemReq *mr) {
  delete mr;
  --outstandingReqs;
  ++completedReqs;
}

void SimpleCpu::instCB(uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
            enum inst_type t)
{
  // Do instruction fetch in icache.
  MemReq *mr = new MemReq(p, 0);
  mr->pushDev(this);
  ++outstandingReqs;
  iCache->req(mr);
}

void SimpleCpu::memCB(uint64_t v, uint64_t p, uint8_t s, int t) {
  // Do memory oeration in dcache.
  MemReq *mr = new MemReq(p, t == 1);
  mr->pushDev(this);
  ++outstandingReqs;
  dCache->req(mr);
}

#undef cba

// TODO: Put SimpleCpu and SuperscalarCpu in different files so each can have its
// own "cba" variable and the following hack can be eliminated:
static CallbackAdaptor<SuperscalarCpu> *cba_superscalarcpu(NULL);
#define cba cba_superscalarcpu

const unsigned SuperscalarCpu::TMP_REG(8);

SuperscalarCpu::SuperscalarCpu(unsigned id, Qsim::OSDomain &osd, unsigned robSz,
                         unsigned issuesPerCycle, unsigned retiresPerCycle) :
  id(id), name(std::string("SuperscalarCpu") + toStr(id)),
  issuesPerCycle(issuesPerCycle), retiresPerCycle(retiresPerCycle),
  issuesLeft(0), fetchReq(NULL), nextUOp(), uOpQueue(), fetchAddr(0),
  robHead(0), robTail(0), robOccupancy(name+std::string("-robOccupancy"), false),
  rob(robSz+1), rat(8), fat(6), memOps(),
  retiredOps(name+std::string("-retiredUOps")),
  fetchedInsts(name+std::string("-fetchedInsts")),
  fuMap(), iCache(NULL), dCache(NULL), inInst(false)
{
  if (!cba) cba = new CallbackAdaptor<SuperscalarCpu>(osd);
  ASSERT(&osd == &cba->osd);
  
  cba->addCpu(this);

  Slide::reg_clock(10, this, &SuperscalarCpu::doRetire);
  Slide::schedule(1000000, this, &SuperscalarCpu::timer, this);
}

void SuperscalarCpu::instCB(uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
                            enum inst_type t)
{
  // Branches are handled in the front-end; as are the control-flow portion of
  // calls and returns.
  if (t == QSIM_INST_BR || t == QSIM_INST_TRAP) t = QSIM_INST_INTBASIC;
  if (t == QSIM_INST_CALL || t == QSIM_INST_RET) t = QSIM_INST_STACK;

  fetchAddr = p;
  nextUOp.type = t;
  inInst = true;

  DBG("Inst@0x" << std::hex << p << std::dec << '\n');

  MemReq *mr = new MemReq(p, false);
  mr->pushDev(this);
  fetchReq = mr;
  DBG("Doing fetch, fetchReq=" << mr << '\n');
  iCache->req(mr);
}

void SuperscalarCpu::memCB(uint64_t v, uint64_t p, uint8_t s, int w)
{
  if (inInst) {
    // Split this instruction into another uOp if it has already accessed
    // memory. Presumably the next operation will depend on the previous one, so
    // link these operations through a the temporary register.
    if (nextUOp.memOp) {
      nextUOp.rDstVec |= (1<<TMP_REG);
      
      uOpQueue.push(nextUOp);
      nextUOp = UOp();
      nextUOp.rSrcVec |= (1<<TMP_REG);
    }

    nextUOp.addr = p;
    nextUOp.wr = w;
    nextUOp.memOp = true;

    DBG("MemOp!\n");
  }
}

void SuperscalarCpu::regCB(int r, uint8_t s, int type)
{
  if (s > 0) {
    if (type) {
      nextUOp.rDstVec |= (1<<r);
    } else {
      nextUOp.rSrcVec |= (1<<r);
    }
  } else {
    (type?nextUOp.fDstVec:nextUOp.fSrcVec) |= r;
  }
}

void SuperscalarCpu::intCB(uint8_t v) {
  // Interrupts cause memory operations to occur outside of instructions. These
  // should not be translated to uOps.
  DBG("Int!\n");
  inInst = false;
}

void SuperscalarCpu::doRetire() {
  DBG("doRetire, cycle " << std::dec << Slide::_now << '\n');

  // Retire as many instructions as possible.
  for (unsigned retiresLeft=retiresPerCycle; retiresLeft && robHead!=robTail;) {
    // Retire all consecutive valid ops at robHead iff they belong to the
    // same instruction and we have enough retires left to retire them.
    int nToRetire = 0, nCandidates = 0;
    for (unsigned i = robHead; i != robTail; i = (i+1)%rob.size()) {
      if (rob[i].valid && rob[i].lastInInst) { 
        DBG("Found a retireable.\n");
        nToRetire += (nCandidates + 1); nCandidates = 0;
        break;
      } 
  
      if (rob[i].valid && !rob[i].lastInInst) { 
        DBG("Found a candidate for retirement.\n");
        ++nCandidates;
      }

      if (!rob[i].valid) {
        DBG("Found an unfinished op. Done.\n");
        break;
      }
    }

    if (nToRetire > retiresLeft || nToRetire == 0) {
      DBG("No more retireables or more to retire than allowed.\n");
      break;
    }

    while (nToRetire-- > 0) {
      DBG("Retiring " << &rob[robHead] << '\n');
      rob[robHead].retire();
      --retiresLeft;
      ++retiredOps;
      robHead = (robHead + 1)%rob.size();
      --robOccupancy;
    }
  }

  DBG(std::dec << Slide::_now << ": retired " << retiredOps << " total. " <<
      robOccupancy << " ROB entries.\n");

  issuesLeft = issuesPerCycle;
  // The next stages are fetch and issue.

  if (uOpQueue.empty()) {
    Slide::schedule(0, this, &SuperscalarCpu::doFetch, 15);
  } else {
    Slide::schedule(0, this, &SuperscalarCpu::doIssue, 17);
  }
}

void SuperscalarCpu::doFetch() {
  if (fetchReq != NULL || !uOpQueue.empty()) return;

  inInst = false;
  cba->osd.run(id, 1);
  ++fetchedInsts;
  nextUOp.lastInInst = true;
  uOpQueue.push(nextUOp);

  // If this is a massive instruction, larger than anything we could hope
  // to retire in a cycle, it will cause a deadlock. We make the assumption
  // that instead, these giant instructions (like FXSAVE) have the same
  // effect if restarted in the event of an exception.
  if (uOpQueue.size() > retiresPerCycle) {
    unsigned size(uOpQueue.size());
    for (unsigned i = 0; i < size; i++) {
      uOpQueue.front().lastInInst = true;
      uOpQueue.push(uOpQueue.front());
      uOpQueue.pop();
    }
  }

  nextUOp = UOp();

  // We now have some uOps in our uOp queue, but we do not advance to the issue
  // stage for these without first getting a response from the icache.
}

void SuperscalarCpu::doIssue() {
  if (fetchReq != NULL) return;
  while (issuesLeft && !uOpQueue.empty()) {
    // We can issue iff there are reservation stations available for this op
    // type and there is free space in the ROB.
    UOp &qHead(uOpQueue.front());
    if (fuMap[qHead.type].rsAvailable > 0 &&
        robHead != (robTail+1)%rob.size())
      {
        // Issue the uOp!
        RobEntry &rEnt(rob[robTail]);
        robTail = (robTail + 1)%rob.size(); ++robOccupancy;
        qHead.dest = &rEnt;
        rEnt.lastInInst = qHead.lastInInst;
        rEnt.type = qHead.type;

        // Use the FAT and RAT to find all source ROB(ARF) entries
        for (unsigned r = 0; r < 8; r++) if ((1<<r)&qHead.rSrcVec) {
          if (rat[r]) qHead.sources.insert(rat[r]);
        }

        for (unsigned f = 0; f < 6; f++) if ((1<<f)&qHead.fSrcVec) {
          if (fat[f]) qHead.sources.insert(fat[f]);
        }

        // Mark the destination registers in the RAT.
        for (unsigned r = 0; r < 8; r++) if ((1<<r)&qHead.rDstVec) {
          if (rat[r]) { rat[r]->ratEntry.erase(&rat[r]); }
          rEnt.ratEntry.insert(&rat[r]);
          rat[r] = &rEnt;
        }

        // Mark the destination flags in the FAT.
        for (unsigned f = 0; f < 6; f++) if ((1<<f)&qHead.fDstVec) {
          if (fat[f]) { fat[f]->fatEntry.erase(&fat[f]); }
          rEnt.fatEntry.insert(&fat[f]);
          fat[f] = &rEnt;
        }

        // "put the op in the reservation station"
        fuMap[qHead.type].rsAvailable--;
        UOp *uOp = new UOp(qHead);

        // If there is a memory operation for this micro op send it out to the 
        // memory system.
        if(qHead.memOp) {
          MemReq *mr = new MemReq(qHead.addr, qHead.wr);
          mr->pushDev(this);
          dCache->req(mr);
          memOps[mr] = uOp;
          uOp->mr = mr;
        }

        Slide::schedule(0, this, &SuperscalarCpu::rsWait, uOp, 20);
        DBG("Issued: ");
        IFDBG(print(std::cout, *uOp));
        DBG(std::dec << issuesLeft-1 << " issues left.\n");

        // Pop the issued uOp and record its issuing.
        fuMap[qHead.type].issuedOps++;
        issuesLeft--;
        uOpQueue.pop();
      }
    else
      {
        // We cannot issue this uOp so we cannot issue any more uOps at all.
        DBG("Finished issuing for cycle " << std::dec << Slide::_now << ".\n" <<
            issuesLeft << " issues left.\n" <<
            uOpQueue.size() << " ops in queue.\n" <<
            fuMap[qHead.type].rsAvailable << " rs available.\n");
        issuesLeft = 0;
        break;
      }
  }

  // TODO: Find a better place to call terminate
  if (cba->finished()) Slide::_terminate();

  // If our uOp queue is empty and there are issues left this cycle, doFetch()
  if (uOpQueue.empty() && issuesLeft > 0) {
    Slide::schedule(0, this, &SuperscalarCpu::doFetch, 5);
    //Slide::schedule(0, this, &SuperscalarCpu::doIssue, 6);
  } else if (!uOpQueue.empty() && issuesLeft > 0) {
    Slide::schedule(0, this, &SuperscalarCpu::doIssue, 5);
  }
}

void SuperscalarCpu::rsWait(SuperscalarCpu::UOp *uOp) {
  using Slide::_now;
  DBG(std::dec << Slide::_now << ": rsWait: ");
  IFDBG(print(std::cout, *uOp));

  FuMapEntry &fu(fuMap[uOp->type]);

  typedef std::set<RobEntry*>::iterator SrcIt;
  bool sourcesReady = true;
  for (SrcIt i = uOp->sources.begin(); i != uOp->sources.end(); ++i) {
    if ((*i)->valid == true) {
      // We have this source now. We can drop it from our set of pending
      //sources.
      DBG("  " << *i << " is ready.\n");
      uOp->sources.erase(*i);
    } else {
      DBG("Can't go yet because I'm waiting on " << *i << "\n");
      sourcesReady = false;
    }
  }

  if (uOp->mr) {
    DBG("Can't go yet because I'm waiting on a memOp.\n");
    sourcesReady = false;
  }

  // Can we initiate this operation yet?
  bool canInitiateNow(fu.nAvailable > 0 &&
                      (_now == 0 || 
                        _now-fu.lastInit[fu.nIIdx] >= fu.initiationInterval-1));

  if (fu.nAvailable == 0)
    DBG("Can't go yet; no FU available.\n");
  else if (_now != 0 && _now-fu.lastInit[fu.nIIdx]-_now<fu.initiationInterval-1)
    DBG("Can't go yet; initiation in this FU too recent.\n");

  if (sourcesReady && canInitiateNow) {
    fu.lastInit[fu.nIIdx] = _now;
    fu.nIIdx = (fu.nIIdx + 1)%fu.nTotal;
    fu.rsAvailable++;
    fu.nAvailable--;
    DBG(std::dec << _now << ": init: ");
    IFDBG(print(std::cout, *uOp));
    Slide::schedule(fu.latency,this,&SuperscalarCpu::opComplete,uOp->dest,20);
    delete uOp;
  } else {
    // Try again next cycle.
    Slide::schedule(1, this, &SuperscalarCpu::rsWait, uOp, 30);
  }
}

void SuperscalarCpu::opComplete(RobEntry *dest) {
  DBG("In opComplete @" << dest << '\n');
  fuMap[dest->type].nAvailable++;
  dest->valid = true;
}

void SuperscalarCpu::resp(MemReq *mr) {
  DBG("Got mr " << mr << ". fetchReq=" << fetchReq << '\n');
  if (mr == fetchReq) {
    // We can now schedule doIssue() on this cycle.
    fetchReq = NULL;
    DBG(Slide::_now << ": Got response for fetch " << mr << '\n');
    Slide::schedule(0, this, &SuperscalarCpu::doIssue, 17);
  } else {
    std::map<MemReq*, SuperscalarCpu::UOp*>::iterator i = memOps.find(mr);
    ASSERT(i != memOps.end());
    i->second->mr = NULL;
    memOps.erase(i);
  }
  delete mr;
}

void SuperscalarCpu::timer(SuperscalarCpu *) {
  cba->osd.timer_interrupt();
 
  Slide::schedule(1000000, this, &SuperscalarCpu::timer, this);
}

const char *SuperscalarCpu::itypeTable[] = {
  "nullop", "intalu", "intmul", "intdiv", "stack",

  // call, ret, and trap instructions do not get their own functional units.
  "Missingno.", "Missingno.", "Missingno.", "Missingno.",

  "fpaddsub", "fpmul", "fpdiv"
};

std::ostream &SuperscalarCpu::print(std::ostream &os,
                                    const SuperscalarCpu::UOp &u)
{
  const char *const R = "ACDBSBsdT";
  const char *const F = "OSZAPC";

  os << SuperscalarCpu::itypeTable[u.type] << ": Src[";
  for (unsigned i=0; i < 9; i++) os << (char)(((1<<i) & u.rSrcVec)?R[i]:'_');
  os << "] Dst[";
  for (unsigned i=0; i < 9; i++) os << (char)(((1<<i) & u.rDstVec)?R[i]:'_');
  os << "] fSrc[";
  for (unsigned i=0; i < 6; i++) os << (char)(((1<<i) & u.fSrcVec)?F[i]:'_');
  os << "] fDst[";
  for (unsigned i=0; i < 6; i++) os << (char)(((1<<i) & u.fDstVec)?F[i]:'_');
  os << "]";

  if (u.lastInInst) os << '*';
  os << ' ' << u.dest;
  if (u.memOp) os << (u.wr?" mWr":" mRd");
  os << '\n';

  return os;
}

#undef cba
