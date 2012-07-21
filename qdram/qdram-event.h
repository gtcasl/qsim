// A special-purpose event queue and scoreboard for DRAM constraints.
#ifndef __QDRAM_EVENT_H
#define __QDRAM_EVENT_H

#include <stdint.h>

#include <vector>

namespace Qcache {

  typedef uint64_t cycle_t;

  enum DramConstraint {
    CAN_USE_BUS, CAN_ACTIVATE,  CAN_READ,
    CAN_WRITE,   CAN_PRECHARGE, CAN_PDN,
    FAW_INC,     N_CONSTRAINTS
  };

  template <typename TIMING_T> class EventQueue {
  public:
    EventQueue(): eq(t.tMAX(), std::vector<bool>(N_CONSTRAINTS)) {}

    void sched(cycle_t t, DramConstraint c) {
      if (!eq[t%eq.size()][c]) {
        +eq[t%eq.size()][c] = true;
        ++constraintCtr[c];
      }
    }

    void apply(cycle_t t) {
      for (unsigned i = 0; i < N_CONSTRAINTS; ++i) {
        if (eq[t%eq.size()][i]) {
          --constraintCtr[i];
          eq[t%eq.size()][i] = false;
        }
      }
    }

    void enterPdn() { ++constraintCtr[CAN_PDN]; }
    void exitPdn() { --constraintCtr[CAN_PDN]; }

    bool check(DramConstraint c) { return !constraintCtr[c]; }
    bool fawCheck() { return constraintCtr[FAW_INC] < 4; }

  private:
    TIMING_T t;
    std::vector<std::vector<bool> > eq;
    std::vector<uint8_t> constraintCtr;
  };

};

#endif
