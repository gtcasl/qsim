#ifndef __QCPU_H
#define __QCPU_H

#include <qsim.h>

#include <map>

#include <qcache.h>

#include <qdram-config.h>
#include <qdram.h>
#include <qdram-sched.h>

#define LLC_LATENCY 30

namespace Qcache {

// Object representing instruction latencies
struct InstLatencyNoforward {
  int getLatency(inst_type t) { return 5; }
  int maxLatency() { return 5; }
};

struct InstLatencyForward {
  int getLatency(inst_type t) { return (t == QSIM_INST_NULL)?2:1; }
  int maxLatency() { return 2; }
};

template <typename TIMINGS, typename MC_T> class CPUTimer {
public:
  CPUTimer(int id, MemSysDev &dMem, MC_T &mc):
    id(id), dMem(&dMem), mc(&mc), cyc(0), now(0), stallCycles(0),
    loadInst(false), notReady(QSIM_N_REGS),
    eq(LLC_LATENCY, std::vector<bool>(QSIM_N_REGS)), dloads(0), xloads(0) {}

  ~CPUTimer() {
    std::cout << "CPU " << id << ": " << now << ", " << cyc << ", " << stallCycles << '\n';
  }

  void instCallback(inst_type type) {
    advance();
    curType = type;

    if (loadInst) {
      // Previous load never got a destination register. Go ahead and issue the
      // load.
      ++xloads;
      dramUseFlag[id] = false;
      int l = dMem->access(loadAddr, loadPc, id, 0);
      loadInst = false;
      if (l < 0) {
        if (~l) std::cout << "Stalled " << ~l << "cyc in destinationless load.\n";
        now += ~l;
        stallCycles += ~l;
      }
    }
  }

  void regCallback(regs r, int wr) {
    if (!wr && notReady[r]) {
      while(notReady[r]) { advance(); ++stallCycles; }
    } else if (wr) {
      int latency;
      if (loadInst) {
        dloads++;
        dramUseFlag[id] = true;
        dramFinishedFlag[id] = notReady.begin() + r;
        int level = dMem->access(loadAddr, loadPc, id, 0);
        if (level < 0) {
          int stallTicks(~level);
          if (stallTicks) {
	    std::cout << "Stalled for " << stallTicks << " ticks.\n";
          }
          now += stallTicks;
          stallCycles += stallTicks;
          latency = 0;
          notReady[r] = true;
        } else if (level == 0) latency = 1;
        else if (level == 1) latency = 10;
        else if (level == 2) latency = 30;
        loadInst = false;
      } else {
        latency = t.getLatency(curType);
      }

      if (latency > 1) {
        notReady[r] = true;
        eq[(cyc + latency)%eq.size()][r] = true;
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
      dramUseFlag[id] = false;
      int l = dMem->access(addr, pc, id, 1);
      if (l < 0) {
        if (~l) std::cout << "Stalled for " << ~l << "cyc in write.\n";
        now += ~l;
        stallCycles += ~l;
      }
    }
  }

private:
  void advance() {
    ++now; ++cyc;

    // Advance the event queue.
    for (unsigned i = 0; i < QSIM_N_REGS; ++i) {
      if (eq[cyc%eq.size()][i]) {
        notReady[i] = false;
        eq[cyc%eq.size()][i] = false;
      }
    }

    // Tick main memory model.
    if (id == 0) mc->lockAndTick();
  }

  TIMINGS t;
  MC_T *mc;
  int id, dloads, xloads;
  cycle_t cyc, now, stallCycles;
  bool loadInst;
  addr_t loadAddr, loadPc;
  inst_type curType;
  MemSysDev *dMem;
  std::vector<bool> notReady;
  std::vector<std::vector<bool> > eq;
};

template <typename MC_T, int ISSUE, int RETIRE, int ROBLEN> class OOOCpuTimer {
public:
  OOOCpuTimer(int id, MemSysDev &dMem, MC_T &mc):
    mc(&mc), id(id), rob(ROBLEN+1), dMem(&dMem), robHead(0), robTail(0), cyc(0),
    now(0), issued(0)
  { std::cout << "id=" << id << " constructed.\n"; }

  ~OOOCpuTimer() {
    std::cout << "CPU " << id << ": " << now << '\n';
  }

  void instCallback(inst_type type) {
    if (loadInst) {
      dramUseFlag[id] = true;
      dramFinishedFlag[id] = rob.begin() + robHead;
      int level = dMem->access(loadAddr, loadPc, id, 0);
      if (level < 0) {
        if (~level) std::cout << "Stalled " << ~level << " cycles in read.\n";
        now += ~level;
        rob[robHead] = true;
      } else {
        if (level == 0) {
          rob[robHead] = false;
        } else if (level == 1) {
          rob[robHead] = true;
          sched(10, rob.begin() + robHead);
        } else if (level == 2) { 
          rob[robHead] = true;
          sched(20, rob.begin() + robHead);
	}
      }
      loadInst = false;
    }

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
      dramUseFlag[id] = false;
      int l = dMem->access(addr, pc, id, 1);
      if (l < 0) {
        if (~l) std::cout << "Stalled " << ~l << " cyc in write.\n";
        now += ~l;
      }
    }
  }

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
      if (rob[robTail] == true) break;
      else if ((robTail+1)%ROBLEN != robHead) robTail = (robTail+1)%ROBLEN;
    }

    //std::cout << now << ": retired " << retired << '\n';

    if (id == 0) mc->lockAndTick();
  }

  MC_T *mc;
  int id, issued;
  cycle_t cyc, now;
  bool loadInst;
  addr_t loadAddr, loadPc;
  MemSysDev *dMem;

  std::vector<bool> rob;
  int robHead, robTail;

  void sched(int ticks, std::vector<bool>::iterator it) {
    eq.insert(std::pair<cycle_t, std::vector<bool>::iterator>(cyc + ticks, it));
  }
  typedef std::multimap<cycle_t, std::vector<bool>::iterator> eq_t;
  eq_t eq;
};

};
#endif
