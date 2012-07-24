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
    id(id), dMem(&dMem), mc(&mc), now(0), stallCycles(0), loadInst(false),
    notReady(QSIM_N_REGS), eq(LLC_LATENCY, std::vector<bool>(QSIM_N_REGS)) {}

  ~CPUTimer() {
    std::cout << "CPU " << id << ": " << now << ", " << stallCycles << '\n';
  }

  void instCallback(inst_type type) {
    advance();
    curType = type;

    if (loadInst) {
      // Previous load never got a destination register. Go ahead and issue the
      // load.
      dramUseFlag[id] = false;
      dMem->access(loadAddr, loadPc, id, 0);
      loadInst = false;
    }
  }

  void regCallback(regs r, int wr) {
    if (!wr && notReady[r]) {
      while(notReady[r]) { advance(); ++stallCycles; }
    } else if (wr) {
      int latency;
      if (loadInst) {
        dramUseFlag[id] = true;
        dramFinishedFlag[id] = notReady.begin() + r;
        int level = dMem->access(loadAddr, loadPc, id, 0);
        if (level == -1) {
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
        eq[(now + latency)%eq.size()][r] = true;
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
      dMem->access(addr, pc, id, 1);
    }
  }

private:
  void advance() {
    now++;

    // TODO: Pre-tick main memory model?

    // Apply all of the event queue latencies.
    for (unsigned i = 0; i < QSIM_N_REGS; ++i) {
      if (eq[now%eq.size()][i]) {
        notReady[i] = false;
        eq[now%eq.size()][i] = false;
      }
    }

    // Tick main memory model.
    if (id == 0 && now%2 == 0) mc->lockAndTick();
  }

  TIMINGS t;
  MC_T *mc;
  int id;
  cycle_t now, stallCycles, loadLatency;
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
    mc(&mc), id(id), rob(ROBLEN+1), dMem(&dMem), robHead(0), robTail(0), now(0),
    issued(0)
  { std::cout << "id=" << id << " constructed.\n"; }

  ~OOOCpuTimer() {
    std::cout << "CPU " << id << ": " << now << '\n';
  }

  void instCallback(inst_type type) {
    if (loadInst) {
      dramUseFlag[id] = true;
      dramFinishedFlag[id] = rob.begin() + robHead;
      int level = dMem->access(loadAddr, loadPc, id, 0);
      if (level == -1) {
        rob[robHead] = true;
      } else {
        if (level == 0) {
          rob[robHead] = false;
        } else if (level == 1) {
          rob[robHead] = true;
          mc->addToFinishQ(5, rob.begin() + robHead);
        } else if (level == 2) { 
          rob[robHead] = true;
          mc->addToFinishQ(10, rob.begin() + robHead);
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
      dMem->access(addr, pc, id, 1);
    }
  }

private:
  void tick() {
    now++;
    issued = 0;

    unsigned retired;
    for (retired = 0; retired < RETIRE && robTail != robHead; ++retired) {
      if (rob[robTail] == true) break;
      else if ((robTail+1)%ROBLEN != robHead) robTail = (robTail+1)%ROBLEN;
    }

    //std::cout << now << ": retired " << retired << '\n';

    if (id == 0 && now%2 == 0) mc->lockAndTick();
  }

  MC_T *mc;
  int id, issued;
  cycle_t now;
  bool loadInst;
  addr_t loadAddr, loadPc;
  MemSysDev *dMem;

  std::vector<bool> rob;
  int robHead, robTail;
};

};
#endif
