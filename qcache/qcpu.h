#ifndef __QCPU_H
#define __QCPU_H

#include <qsim.h>

#include <map>

#include <qcache.h>
#include <qtickable.h>

#include <qdram-config.h>
#include <qdram.h>
#include <qdram-sched.h>

namespace Qcache {

typedef uint64_t cycle_t;

// Object representing instruction latencies
struct InstLatencyNoforward {
  int getLatency(inst_type t) { return 5; }
  int maxLatency() { return 5; }
};

struct InstLatencyForward {
  int getLatency(inst_type t) { return (t == QSIM_INST_NULL)?2:1; }
  int maxLatency() { return 2; }
};

template <typename TIMINGS> class CPUTimer {
public:
 CPUTimer(int id, MemSysDev &dMem, MemSysDev &iMem, Tickable *mc=NULL):
    id(id), dMem(&dMem), cyc(0), now(0), stallCycles(0),
    loadInst(false), iMem(&iMem), mc(mc),
    eq(dMem.getLatency(), std::vector<bool>(QSIM_N_REGS)), dloads(0), xloads(0)
  { for (unsigned i = 0; i < QSIM_N_REGS; ++i) notReady[i] = false; }

  ~CPUTimer() {
    if (printResults)
      std::cout << "CPU " << id << ": " << now << ", " << cyc << ", "
                << stallCycles << '\n';
  }

  void idleInst() { advance(); }

  void instCallback(addr_t addr, inst_type type) {
    advance();
    curType = type;

    if (loadInst) {
      // Previous load never got a destination register. Go ahead and issue the
      // load.
      ++xloads;
      dMem->access(loadAddr, loadPc, id, 0);
      loadInst = false;
    }

    int latency = iMem->access(addr, addr, id, 0, &instFlag[0]);
    if (latency < 0) {
      instFlag[0] = true;
      MEM_BARRIER();
      while (instFlag[0]) { advance(); ++stallCycles; MEM_BARRIER(); }
    } else {
      for (unsigned i = 0; i < latency; ++i) { advance(); ++stallCycles; }
    }
  }

  void regCallback(regs r, int wr) {
    if (!wr && notReady[r]) {
      MEM_BARRIER();
      while(notReady[r]) { advance(); ++stallCycles; MEM_BARRIER(); }
    } else if (wr) {
      int latency;
      notReady[r] = true;
      if (loadInst) {
        dloads++;
        latency = dMem->access(loadAddr, loadPc, id, 0, &notReady[r]);
      } else {
        latency = t.getLatency(curType);
      }

      if (latency > 0) {
        notReady[r] = true;
        eq[(cyc + latency)%eq.size()][r] = true;
      } else if (latency == 0) {
        notReady[r] = false;
      }

      if (loadInst) loadInst = false;
    }
  }

  void memCallback(uint64_t addr, uint64_t pc, int wr) {
    if (!wr) {
      loadInst = true;
      loadAddr = addr;
      loadPc = pc;
    } else {
      // Writes are not on the critical path.
      dMem->access(addr, pc, id, 1);
    }
  }

  cycle_t getCycle() { return now; }

private:
  void advance() {
    ++now; ++cyc;

    if (mc) mc->tick();

    // Advance the event queue.
    for (unsigned i = 0; i < QSIM_N_REGS; ++i) {
      if (eq[cyc%eq.size()][i]) {
        notReady[i] = false;
        eq[cyc%eq.size()][i] = false;
      }
    }
  }

  TIMINGS t;
  int id, dloads, xloads;
  cycle_t cyc, now, stallCycles;
  bool loadInst;
  addr_t loadAddr, loadPc;
  inst_type curType;
  MemSysDev *dMem, *iMem;
  bool notReady[QSIM_N_REGS], instFlag[1];
  std::vector<std::vector<bool> > eq;
  Tickable *mc;
};

template <int ISSUE, int RETIRE, int ROBLEN> class OOOCpuTimer {
public:
  OOOCpuTimer(int id, MemSysDev &dMem, MemSysDev &iMem):
    id(id), dMem(&dMem), robHead(0), robTail(0), cyc(0),
    iMem(&iMem), now(0), issued(0)
  { for (unsigned i = 0; i < ROBLEN; ++i) rob[i] = false; }

  ~OOOCpuTimer() {
    if (printResults) std::cout << "CPU " << id << ": " << now << '\n';
  }

  void idleInst() { tick(); }

  void instCallback(addr_t addr, inst_type type) {
    if (loadInst) {
      int latency = dMem->access(loadAddr, loadPc, id, 0);
      if (latency < 0) {
        rob[robHead] = true;
      } else if (latency == 0) {
        rob[robHead] = false;
      } else if (latency > 0) {
        rob[robHead] = true;
        sched(latency, &rob[robHead]);
      }
      loadInst = false;
    }

    int latency = iMem->access(addr, addr, id, 0);
    if (latency > 0) {
      instFlag[0] = true;
      sched(latency, instFlag);
    } else if (latency == 0) {
      instFlag[0] = false;
    }
    
    while (instFlag[0]) tick();

    if (issued >= ISSUE) tick();

    while ((robHead+1)%ROBLEN == robTail) tick();

    robHead = (robHead+1)%ROBLEN;

  }

  void regCallback(regs r, int wr) {}

  void memCallback(uint64_t addr, uint64_t pc, int wr) {
    if (!wr) {
      loadInst = true;
      loadAddr = addr;
      loadPc = pc;
    } else {
      // Writes are not on the critical path.
      dMem->access(addr, pc, id, 1);
    }
  }

  cycle_t getCycle() { return now; }

private:
  void tick() {
    now++; cyc++;

    eq_t::iterator it;
    while ((it = eq.find(cyc)) != eq.end()) {
      *(it->second) = false;
      eq.erase(it);
    }

    issued = 0;
    unsigned retired;
    for (retired = 0; retired < RETIRE && robTail != robHead; ++retired) {
      MEM_BARRIER();
      if (rob[robTail] == true) break;
      MEM_BARRIER();
      if ((robTail+1)%ROBLEN != robHead) robTail = (robTail+1)%ROBLEN;
    }

    //std::cout << now << ": retired " << retired << '\n';
  }

  int id, issued;
  cycle_t cyc, now;
  bool loadInst;
  addr_t loadAddr, loadPc;
  MemSysDev *dMem, *iMem;

  bool instFlag[1], rob[ROBLEN+1];
  int robHead, robTail;

  void sched(int ticks, bool* it) {
    eq.insert(std::pair<cycle_t, bool*>(cyc + ticks, it));
  }
  typedef std::multimap<cycle_t, bool*> eq_t;
  eq_t eq;
};

};
#endif
