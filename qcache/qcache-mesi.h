#ifndef __QCACHE_MESI_H
#define __QCACHE_MESI_H

#include <iostream>
#include <iomanip>

#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>
#include <limits.h>

#include <stdlib.h>

#include "qcache.h"
#include "qcache-dir.h"

namespace Qcache {
  // Directory MESI coherence protocol.
  template <int L2LINESZ, typename CACHE> class CPDirMesi {
  public:
    CPDirMesi(std::vector<CACHE> &caches): caches(caches) {}

    enum State {
      STATE_I = 0x00,
      STATE_X = 0x01, // Initial state
      STATE_M = 0x02,
      STATE_E = 0x03,
      STATE_S = 0x04
    };

    void lockAddr(addr_t addr, int id)   { dir.lockAddr(addr, id); }
    void unlockAddr(addr_t addr, int id) { dir.unlockAddr(addr, id); }
    void addAddr(addr_t addr, int id) { dir.addAddr(addr, id); }
    void remAddr(addr_t addr, int id) { dir.remAddr(addr, id); }

    bool hitAddr(int id, addr_t addr, bool locked,
                 spinlock_t *setLock, uint64_t *line, bool wr)
    {
      if (getState(line) == STATE_M || (getState(line) == STATE_E)) {
        if (getState(line) == STATE_E && wr) {
          setState(line, STATE_M);
	}
	spin_unlock(setLock);
        return true;
      } else if (getState(line)) {
        spin_unlock(setLock);
        if (!wr) return true;

        // If I don't hold the lock for this address, get it.
        if (!locked) {
          lockAddr(addr, id);
	}

        // Since we have to serialize on the address lock, it is still possible
        // for a miss to occur
        if (!dir.hasId(addr, id)) {
          if (locked) {
            pthread_mutex_lock(&errLock);
	    std::cerr << "Hit 0x" << std::hex << addr << " on " << id
                      << " missing from dir. Dir entry:";
	    for (std::set<int>::iterator it = dir.idsBegin(addr, id);
                 it != dir.idsEnd(addr, id); ++it)
	    {
	      std::cout << ' ' << *it;
	    }
	    std::cerr << '\n';
	    std::cerr << "Cache set:"; caches[id].dumpSet(addr);
            pthread_mutex_unlock(&errLock);
            ASSERT(false);
	  }
          unlockAddr(addr, id);
          return false;
        }

        // Invalidate all of the remote lines.
	for (std::set<int>::iterator it = dir.idsBegin(addr, id);
	     it != dir.idsEnd(addr, id); ++it)
	{
          if (*it == id) continue;
          spinlock_t *l;
          uint64_t *invLine = caches[*it].cprotLookup(addr, l, true);
          *invLine = *invLine & ~(uint64_t)((1<<L2LINESZ)-1);
          caches[*it].invalidateLowerLevel(addr);
          spin_unlock(l);
        }
        dir.clearIds(addr, id);

        #ifdef DEBUG
        pthread_mutex_lock(&errLock);
	std::cout << id << ": 0x" << std::hex << addr << " S->M\n";
        pthread_mutex_unlock(&errLock);
        #endif

        setState(line, STATE_M);

        ASSERT(dir.hasId(addr, id));

        if (!locked) {
          unlockAddr(addr, id);
        }

        return true;
      } else {
	std::cerr << "Invalid state: " << getState(line) << '\n';
        ASSERT(false); // Invalid state.
        return false;
      }
    }

    bool missAddr(int id, addr_t addr, uint64_t *line, bool wr) {
      State st;
      addAddr(addr, id);

      bool forwarded = false;
   
      #ifdef DEBUG
      pthread_mutex_lock(&errLock);
      std::cout << id << ": 0x" << std::hex << addr << " I->"
                << (wr?"M\n":"S\n");
      pthread_mutex_unlock(&errLock);
      #endif

      // Invalidate all of the remote lines.
      bool shared(false);
      for (std::set<int>::iterator it = dir.idsBegin(addr, id);
           it != dir.idsEnd(addr, id); ++it)
      {
        if (*it == id) continue;

        shared = true;
        forwarded = true;

        spinlock_t *l;
        uint64_t *remLine = caches[*it].cprotLookup(addr, l, wr);
        if (wr) {
          *remLine = *remLine & ~(uint64_t)((1<<L2LINESZ)-1);
          caches[*it].invalidateLowerLevel(addr);
        } else {
          *remLine = *remLine & ~(uint64_t)((1<<L2LINESZ)-1) | STATE_S;
        }
        spin_unlock(l);
      }
      if (wr) dir.clearIds(addr, id);

      if (wr)          st = STATE_M;
      else if (shared) st = STATE_S;
      else             st = STATE_E;

      setState(line, st);

      ASSERT(dir.hasId(addr, id));

      return forwarded;
    }

    bool evAddr(int id, addr_t addr, int state) {
      remAddr(addr, id);
      return state == STATE_M;
    }

  private:
    CoherenceDir<L2LINESZ> dir;

    void setState(uint64_t *line, State state) {
      *line = *line & ((~(uint64_t)0)<<L2LINESZ) | state;
      ASSERT(getState(line) == state);
    }

    State getState(uint64_t *line) {
      return State(*line & ((1<<L2LINESZ)-1));
    }

    std::vector<CACHE> &caches;
  };
};

#endif
