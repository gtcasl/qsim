#ifndef __QDRAM_SCHED
#define __QDRAM_SCHED

#include <qcache.h>
#include <qtickable.h>

#include <pthread.h>

#include <iostream>
#include <list>
#include <map>

#include "qdram.h"

namespace Qcache {
  #define LAT_OFFSET INT_MIN

  struct req_t {
    req_t(addr_t addr): a(addr), s(false) {}
    req_t(addr_t addr, bool* flag):
      a(addr), f(flag), s(true) {}
    bool *f;
    addr_t a;
    bool s;
  };

  template <typename TIMING_T, typename DIM_T,
            template<typename> class ADDRMAP_T, int CACHE_LATENCY, int TSCAL>
    class MemController : 
      public MemSysDev, public Tickable
  {
  public:
    MemController(int c) :
      ticks(0), subticks(0), accesses(0), activates(0),
      rqlen(64000), wqlen(64000), hwm(50), lwm(10), allTimeExtra(0), cores(c)
    {
      pthread_mutex_init(&lock, NULL);
    }

    ~MemController() { printStats(); }

    void printStats() {
      std::cout << "DRAM: " << ticks << " ticks, " << activates
                << " activates, " << accesses << " accesses, " << allTimeExtra 
                << " stall cycles\n";
    }

    int access(
      addr_t addr, addr_t pc, int core, int wr, bool *flagptr, addr_t** lp=NULL
    ) {
      int extraCyc(0);
      pthread_mutex_lock(&lock);

      while (wr && wrq.size() >= wqlen) { tickEnd(); tickBegin(); ++extraCyc; }
      while (!wr && rdq.size() >= rqlen) { tickEnd(); tickBegin(); ++extraCyc; }

      if (wr) {
        wrq.push_back(req_t(addr));
        if (wrq.size() >= hwm) writeMode = true;
      } else {
        if (flagptr) rdq.push_back(req_t(addr, flagptr));
        else         rdq.push_back(req_t(addr));
      }

      allTimeExtra += extraCyc;

      pthread_mutex_unlock(&lock);

      return LAT_OFFSET;
    }

    int getLatency() { return 0; }

    bool empty() { return wrq.empty() && rdq.empty(); }

    void tick() {
      pthread_mutex_lock(&lock);
      tickEnd();
      tickBegin();
      pthread_mutex_unlock(&lock);
    }

    void tickBegin() {
      if (++subticks != TSCAL*cores) return;
      std::map<cycle_t, bool*>::iterator it;
      while ((it = finishQ.find(ticks)) != finishQ.end()) {
        *(it->second) = false;
        finishQ.erase(it);
        MEM_BARRIER();
      }
      ch.tickBegin();
    }

    void tickEnd() {
      if (subticks != TSCAL*cores) return;
      subticks = 0;
      tickSched(); ch.tickEnd(); ++ticks;
    }

    cycle_t getCycle() {
      return ticks*TSCAL + subticks/cores;
    }

  private:
    Channel<TIMING_T, DIM_T, ADDRMAP_T> ch;
    cycle_t ticks;
    int subticks, cores;
    uint64_t accesses, activates;
    ADDRMAP_T<DIM_T> m;

    pthread_mutex_t lock;

    std::list<req_t> rdq, wrq;
    bool writeMode;
    int rqlen, wqlen, hwm, lwm, allTimeExtra;

    std::multimap<cycle_t, bool*> finishQ;

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
            finishQ.insert(std::pair<cycle_t, bool*>(
              ticks + ch.t.tCL() + 4 + CACHE_LATENCY/TSCAL, i->f
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
