#ifndef __QDRAM_SCHED
#define __QDRAM_SCHED

#include <qcache.h>

#include <pthread.h>

#include <iostream>
#include <list>
#include <map>

#include "qdram.h"

namespace Qcache {
  #define LAT_OFFSET INT_MIN

  struct req_t {
    req_t(addr_t addr): a(addr), s(false) {}
    req_t(addr_t addr, std::vector<bool>::iterator flag):
      a(addr), f(flag), s(true) {}
    std::vector<bool>::iterator f;
    addr_t a;
    bool s;
  };

  template <typename TIMING_T, typename DIM_T,
            template<typename> class ADDRMAP_T, int TSCAL>
  class MemController : public MemSysDev {
  public:
    MemController() :
      ticks(0), subticks(0), accesses(0), activates(0),
      rqlen(64000), wqlen(64000), hwm(50), lwm(10), allTimeExtra(0)
    {
      pthread_mutex_init(&lock, NULL);
    }

    ~MemController() { printStats(); }

    void printStats() {
      std::cout << "DRAM: " << ticks << " ticks, " << activates
                << " activates, " << accesses << " accesses, " << allTimeExtra 
                << '\n';
    }

    int access(addr_t addr, addr_t pc, int core, int wr, addr_t** lp=NULL) {
      int extraCyc(0);
      pthread_mutex_lock(&lock);

      std::cout << "DRAM access: " << (wr?"-1":"1") << ", " << TSCAL*ticks << ", " << rdq.size() << ", " << wrq.size() << '\n';

      while (wr && wrq.size() >= wqlen) { tickEnd(); tickBegin(); ++extraCyc; }
      while (!wr && rdq.size() >= rqlen) { tickEnd(); tickBegin(); ++extraCyc; }

      if (wr) {
        wrq.push_back(req_t(addr));
        if (wrq.size() >= hwm) writeMode = true;
      } else {
        if (dramUseFlag[core])
          rdq.push_back(req_t(addr, dramFinishedFlag[core]));
        else
          rdq.push_back(req_t(addr));
      }

      allTimeExtra += extraCyc;

      pthread_mutex_unlock(&lock);

      return LAT_OFFSET;
    }

    int getLatency() { return 0; }

    bool empty() { return wrq.empty() && rdq.empty(); }

    void lockAndTick() {
      pthread_mutex_lock(&lock);
      tickEnd();
      tickBegin();
      pthread_mutex_unlock(&lock);
    }

    void tickBegin() {
      if (++subticks != TSCAL) return;
      std::map<cycle_t, std::vector<bool>::iterator>::iterator it;
      while ((it = finishQ.find(ticks)) != finishQ.end()) {
        *(it->second) = false;
        finishQ.erase(it);
      }
      ch.tickBegin();
    }

    void tickEnd() {
      if (subticks != TSCAL) return;
      subticks = 0;
      tickSched(); ch.tickEnd(); ++ticks;
    }

  private:
    Channel<TIMING_T, DIM_T, ADDRMAP_T> ch;
    cycle_t ticks;
    int subticks;
    uint64_t accesses, activates;
    ADDRMAP_T<DIM_T> m;

    pthread_mutex_t lock;

    std::list<req_t> rdq, wrq;
    bool writeMode;
    int rqlen, wqlen, hwm, lwm, allTimeExtra;

    std::multimap<cycle_t, std::vector<bool>::iterator> finishQ;

    void tickSched() {
      // Do not allow any request in the queue to make reverse progress.
      std::vector<std::vector<bool> >
        dnp(m.d.ranks(), std::vector<bool>(m.d.banks()));

      for (std::list<req_t>::iterator i = rdq.begin(); i != rdq.end(); ++i) {
        if (ch.rowHit(i->a)) dnp[m.getRank(i->a)][m.getBank(i->a)] = true;
        if (ch.canRead(i->a)) {
          ++accesses;
          ch.issueRead(i->a);
          if (i->s) {
            finishQ.insert(std::pair<cycle_t, std::vector<bool>::iterator>(
              ticks + ch.t.tCL() + 4 + dramAdditionalLatency/TSCAL, i->f
            ));
          }
          rdq.erase(i);
          return;
        }
      }

      if (writeMode || rdq.empty()) {
        for (std::list<req_t>::iterator i = wrq.begin(); i != wrq.end(); ++i) {
          if (ch.rowHit(i->a)) dnp[m.getRank(i->a)][m.getBank(i->a)] = true;
          if (ch.canWrite(i->a)) {
            ++accesses;
            ch.issueWrite(i->a);
            wrq.erase(i);
            if (wrq.size() <= lwm) writeMode = false;
            return;
          }
        }

        for (std::list<req_t>::iterator i = wrq.begin(); i != wrq.end(); ++i) {
          if (!dnp[m.getRank(i->a)][m.getBank(i->a)]&&ch.mustPrecharge(i->a) &&
              ch.canPrecharge(i->a))
	  {
            ch.issuePrecharge(i->a);
            return;
	  } else if (ch.canActivate(i->a)) {
            ++activates;
            ch.issueActivate(i->a);
          }
        }
      }

      for (std::list<req_t>::iterator i = rdq.begin(); i != rdq.end(); ++i) {
        if (!dnp[m.getRank(i->a)][m.getBank(i->a)] && ch.mustPrecharge(i->a) &&
            ch.canPrecharge(i->a))
        {
          ch.issuePrecharge(i->a);
          return;
	} else if (ch.canActivate(i->a)) {
          ++activates;
          ch.issueActivate(i->a);
        }
      }
    }

  };

};

#endif
