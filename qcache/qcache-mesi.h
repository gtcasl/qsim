#if !__QCACHE_MESI_H || __QCACHE_DEF_MSI || __QCACHE_DEF_MOESI
#define __QCACHE_MESI_H 1

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
  template <int L2LINESZ, typename CACHE> class
#ifdef __QCACHE_DEF_MSI
    CPDirMsi
#else
#ifdef __QCACHE_DEF_MOESI
    CPDirMoesi
#else
    CPDirMesi
#endif
#endif
  {

  public:
#ifdef __QCACHE_DEF_MSI
    CPDirMsi(std::vector<CACHE>&caches):
#else
#ifdef __QCACHE_DEF_MOESI
    CPDirMoesi(std::vector<CACHE> &caches):
#else
    CPDirMesi(std::vector<CACHE> &caches):
#endif
#endif
      caches(caches) {}

    enum State {
      STATE_I = 0x00,
      STATE_X = 0x01, // Initial state
      STATE_M = 0x02,
      STATE_O = 0x03,
      STATE_E = 0x04,
      STATE_S = 0x05
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
	if (setLock) spin_unlock(setLock);
        return true;
      } else if (getState(line)) {
        if (setLock) spin_unlock(setLock);
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
          setState(invLine, STATE_I);
          spin_unlock(l);
        }
        dir.clearIds(addr, id);

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
      if (dir.hasId(addr, id)) {
        // It's in a lower-level private cache on the same core.
        hitAddr(id, addr, true, NULL, line, wr);
        return false;
      }

      State st;
      addAddr(addr, id);

      bool forwarded = false;
   
      // Change state of remote lines.
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
          setState(remLine, STATE_I);
        } else {
#ifdef __QCACHE_DEF_MOESI
          // MOESI doesn't have to do a writeback yet.
          if (getState(remLine) == STATE_M || getState(remLine) == STATE_E ||
              getState(remLine) == STATE_O)
          {
            setState(remLine, STATE_O);
          } else {
            setState(remLine, STATE_S);
          }
#else
          // This line has to be written back.
          if (getState(remLine) == STATE_M && caches[*it].lowerLevel) {
            caches[*it].lowerLevel->access(addr, true);
          }

          setState(remLine, STATE_S);
#endif
        }
        spin_unlock(l);
      }
      if (wr) dir.clearIds(addr, id);

      if (wr)          st = STATE_M;
#ifdef __QCACHE_DEF_MSI
      else             st = STATE_S;
#else
      else if (shared) st = STATE_S;
      else             st = STATE_E;
#endif

      setState(line, st);

      ASSERT(dir.hasId(addr, id));

      return forwarded;
    }

    void evAddr(int id, addr_t addr) {
      remAddr(addr, id);
    }

    bool dirty(int state) {
      return state == STATE_M || state == STATE_O;
    }

  private:
    CoherenceDir<L2LINESZ> dir;

    void setState(uint64_t *line, State state) {
#ifdef DEBUG
      pthread_mutex_lock(&errLock);
      addr_t addr = *line & ~((1<<L2LINESZ)-1);
      int curSt = *line & ((1<<L2LINESZ)-1), nextSt = int(state);
      const char STATES[]  = {'I', 'X', 'M', 'O', 'E', 'S'};
      std::cout << "0x" << std::hex << addr << ' '
                << STATES[curSt] << "->" << STATES[nextSt] << '\n';
      pthread_mutex_unlock(&errLock);
#endif
      *line = *line & ~(uint64_t)((1<<L2LINESZ)-1) | state;
      ASSERT(getState(line) == state);
    }

    State getState(uint64_t *line) {
      return State(*line & ((1<<L2LINESZ)-1));
    }

    std::vector<CACHE> &caches;
  };
};

#endif
