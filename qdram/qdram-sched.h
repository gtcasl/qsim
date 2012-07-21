#ifndef __QDRAM_SCHED
#define __QDRAM_SCHED

#include <qcache.h>

#include <iostream>
#include <list>

#include "qdram.h"

namespace Qcache {

  template <typename TIMING_T, typename DIM_T,
            template<typename> class ADDRMAP_T>
  class MemController {
  public:
    MemController() : ticks(0), accesses(0), activates(0),
                      rqlen(64), wqlen(64), hwm(50), lwm(2) {}

    void printStats() {
      std::cout << "DRAM: " << ticks << " ticks, " << activates
                << " activates, " << accesses << " accesses\n";
    }

    bool access(addr_t addr, bool wr) {
      if (wr && wrq.size() >= wqlen) return false;
      if (!wr && rdq.size() >= rqlen) return false;

      if (wr) {
        wrq.push_back(addr);
        if (wrq.size() >= hwm) writeMode = true;
      } else {
        rdq.push_back(addr);
      }

      return true;
    }

    bool empty() { return wrq.empty() && rdq.empty(); }

    void tickBegin() { ch.tickBegin(); }
    void tickEnd() { tickSched(); ch.tickEnd(); }

  private:
    Channel<TIMING_T, DIM_T, ADDRMAP_T> ch;
    cycle_t ticks;
    uint64_t accesses, activates;
    ADDRMAP_T<DIM_T> m;

    std::list<addr_t> rdq, wrq;
    bool writeMode;
    int rqlen, wqlen, hwm, lwm;

    void tickSched() {
      // Do not allow any request in the queue to make reverse progress.
      std::vector<std::vector<bool> >
        dnp(m.d.ranks(), std::vector<bool>(m.d.banks()));

      for (std::list<addr_t>::iterator i = rdq.begin(); i != rdq.end(); ++i) {
        if (ch.rowHit(*i)) dnp[m.getRank(*i)][m.getBank(*i)] = true;
        if (ch.canRead(*i)) {
          ++accesses;
          ch.issueRead(*i);
          rdq.erase(i);
          return;
        }
      }

      if (writeMode || rdq.empty()) {
        for (std::list<addr_t>::iterator i = wrq.begin(); i != wrq.end(); ++i) {
          if (ch.rowHit(*i)) dnp[m.getRank(*i)][m.getBank(*i)] = true;
          if (ch.canWrite(*i)) {
            ++accesses;
            ch.issueWrite(*i);
            wrq.erase(i);
            if (wrq.size() <= lwm) writeMode = false;
            return;
          }
        }

        for (std::list<addr_t>::iterator i = wrq.begin(); i != wrq.end(); ++i) {
          if (!dnp[m.getRank(*i)][m.getBank(*i)] && ch.mustPrecharge(*i) &&
              ch.canPrecharge(*i))
	  {
            ch.issuePrecharge(*i);
            return;
	  } else if (ch.canActivate(*i)) {
            ++activates;
            ch.issueActivate(*i);
          }
        }
      }

      for (std::list<addr_t>::iterator i = rdq.begin(); i != rdq.end(); ++i) {
        if (!dnp[m.getRank(*i)][m.getBank(*i)] && ch.mustPrecharge(*i) &&
            ch.canPrecharge(*i))
        {
          ch.issuePrecharge(*i);
          return;
	} else if (ch.canActivate(*i)) {
          ++activates;
          ch.issueActivate(*i);
        }
      }
    }

  };

};

#endif
