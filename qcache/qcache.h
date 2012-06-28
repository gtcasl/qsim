#ifndef __QCACHE_H
#define __QCACHE_H

#include <iostream>
#include <iomanip>

#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>
#include <limits.h>

#include <stdlib.h>

//#define DEBUG
#define ENABLE_ASSERTIONS

#ifdef ENABLE_ASSERTIONS
#define ASSERT(b) do { if (!(b)) { \
  std::cerr << "Error: Failed assertion at " << __FILE__ << ':' << std::dec \
            << __LINE__ << '\n'; \
  abort(); \
} } while(0)

#else
#define ASSERT(b) do {} while(0)
#endif

namespace Qcache {
  extern pthread_mutex_t errLock;
  extern bool printResults;

  const unsigned DIR_BANKS = 256; // # coherence directory banks

  typedef pthread_spinlock_t spinlock_t;
  #define spinlock_init(s) do { pthread_spin_init((s), 0); } while (0)
  #define spin_lock(s) do { pthread_spin_lock((s)); } while (0)
  #define spin_unlock(s) do { pthread_spin_unlock((s)); } while (0)
  typedef uint64_t addr_t;

  // Things to throw when we're angry.
  struct InvalidAccess {};

  // Every level in the memory hierarchy is one of these.
  class MemSysDev {
   public:
    virtual ~MemSysDev() {}

    virtual bool access(addr_t addr, bool wr) {
      throw InvalidAccess();
      return false;
    }

    virtual void invalidate(addr_t addr) { throw InvalidAccess(); }
    virtual bool isShared() { return false; }
  };

  class MemSysDevSet {
   public:
    virtual ~MemSysDevSet() {}
    virtual MemSysDev &getMemSysDev(size_t i)=0;
  };

  // Place one of these at any level in the hierarchy to get a read/write trace
  // at that level.
  class Tracer : public MemSysDev {
   public:
    Tracer(std::ostream &tf) : tracefile(tf) {}

    bool access(addr_t addr, bool wr) {
      tracefile << std::dec << addr << (wr?" W\n":" R\n");
      return false;
    }

   private:
    std::ostream &tracefile;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplRand {
  public:
    ReplRand(std::vector<addr_t> &ta): tagarray(&ta[0]) {}

    void updateRepl(addr_t set, addr_t idx, bool hit, bool wr) {}

    addr_t findVictim(addr_t set) {
      // First: look for invalid lines.
      for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i)
        if (!(tagarray[i] & ((1<<L2LINESZ)-1))) return i;

      // If none are found, go rando. This is designed to be highly uniform,
      // since favoring certain ways could have interesting consequences.
      int way;
      do { way = rand(); } while (way > RAND_MAX/WAYS*WAYS);
      way %= WAYS;
    
      return set*WAYS + way;
    }
    
  private:
    addr_t *tagarray;
  };

  // Caches, private or shared, of any dimension
  template
    <template<int, typename> class CPROT_T,
    int WAYS, int L2SETS, int L2LINESZ, template<int, int, int> class REPL_T,
    bool SHARED=false>
  class Cache : public MemSysDev
  {
   public:
     Cache(MemSysDev &ll, const char *n = "Unnamed",
           CPROT_T<L2LINESZ, Cache> *cp = NULL) :
      tagarray(size_t(WAYS)<<L2SETS), repl(tagarray),
      peers(NULL), lowerLevel(&ll), cprot(cp), id(0), name(n),
      accesses(0), misses(0), invalidates(0)
    {
      initArrays();
    }

    Cache(std::vector<Cache> &peers, MemSysDev &ll, int id,
          const char *n = "Unnamed", CPROT_T<L2LINESZ, Cache> *cp = NULL) :
      tagarray(WAYS<<L2SETS), repl(tagarray),
      peers(&peers), lowerLevel(&ll), cprot(cp), id(id), name(n),
      accesses(0), misses(0), invalidates(0)
    {
      initArrays();
    }

    ~Cache() {
      if (!printResults) return;
      std::cout << name << ", " << id << ", " << accesses << ", " << misses 
                << ", " << invalidates << '\n';
    }

    bool access(addr_t addr, bool wr) {
      bool hit = false;

      ++accesses;
      if (SHARED) spin_lock(&accessLock);

      addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
             set(tag%(1<<L2SETS));

      addr &= ~stateMask; // Throw away address LSBs.

      spin_lock(&setLocks[set]);
      addr_t idx;
      for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
        if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {
          repl.updateRepl(set, idx, true, wr);
          hit = true;
          if (cprot->hitAddr(id, addr, false, &setLocks[set],
                             &tagarray[idx], wr)) goto finish;
          else {
            spin_lock(&setLocks[set]);
            break;
	  }
        }
      }
      ++misses;

      size_t vidx;
      addr_t victimAddr;
      int victimState;
      uint64_t victimVal;
      for (;;) {
        vidx = repl.findVictim(set);
        victimVal = tagarray[vidx];
        victimAddr = tagarray[vidx] & ~stateMask;
        victimState = tagarray[vidx] & stateMask;
        ASSERT(victimState == 0 || victimAddr != addr);
        spin_unlock(&setLocks[set]);
        
        // Lock access block and victim block (if not an invalid line) in order
        if (victimState && victimAddr < addr) cprot->lockAddr(victimAddr, id);
        cprot->lockAddr(addr, id);
        if (victimState && victimAddr > addr) cprot->lockAddr(victimAddr, id);

        spin_lock(&setLocks[set]);
        for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
          if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {
            // The block we were looking for made its way into the cache.
            repl.updateRepl(set, idx, true, wr);
            cprot->hitAddr(id, addr, true, &setLocks[set], &tagarray[idx], wr);
            hit = true;
            goto missFinish;
          }
        }

        // The block we were looking for is still not in the cache.

        if (!victimState && (tagarray[vidx] & stateMask)) {
          // Our invalid victim became valid.
          spin_unlock(&setLocks[set]);
          cprot->unlockAddr(addr, id);
          spin_lock(&setLocks[set]);
          continue;
	}

        if (victimState && tagarray[vidx] != victimVal) {
          // Victim has changed in the array.
          spin_unlock(&setLocks[set]);
          if (victimAddr > addr) cprot->unlockAddr(victimAddr, id);
          cprot->unlockAddr(addr, id);
          if (victimAddr < addr) cprot->unlockAddr(victimAddr, id);
          spin_lock(&setLocks[set]);
          continue;
	}

        #ifdef DEBUG
        pthread_mutex_lock(&errLock);
	std::cout << id << ": Found victim: 0x" << std::hex << tagarray[vidx]
                  << '\n';
        pthread_mutex_unlock(&errLock);
        #endif
        
        break;
      }
      tagarray[vidx] = addr|0x01;
      repl.updateRepl(set, vidx, false, wr);
      spin_unlock(&setLocks[set]);
      if (victimState) {
        bool doWriteback = cprot->evAddr(id, victimAddr, victimState);
        if (doWriteback && lowerLevel) lowerLevel->access(victimAddr, true);
      }

      if (!cprot->missAddr(id, addr, &tagarray[vidx], wr) && lowerLevel) {
        lowerLevel->access(tag<<L2LINESZ, false);
      }

    missFinish:
      // Unlock access block and victim block in reverse order.
      if (victimState && victimAddr > addr) cprot->unlockAddr(victimAddr, id);
      cprot->unlockAddr(addr, id);
      if (victimState && victimAddr < addr) cprot->unlockAddr(victimAddr, id);

    finish:
      if (SHARED) spin_unlock(&accessLock);

      return hit;
    }

    void invalidate(addr_t addr) {
      ASSERT(!SHARED);

      addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
             set(tag%(1<<L2SETS));

      spin_lock(&setLocks[set]);
      addr_t idx;
      for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
        if ((tagarray[idx]>>L2LINESZ)==tag) {
          tagarray[idx] = 0;
          // No need to get invalidates lock since we are not shared and are
          // not accessed by the coherence protocol.
          ++invalidates;
        }
      }
      spin_unlock(&setLocks[set]);
    }

    bool isShared() { return SHARED; }

    void invalidateLowerLevel(addr_t addr) {
      ASSERT(!SHARED);

      if (lowerLevel && !lowerLevel->isShared()) lowerLevel->invalidate(addr);
    }

   // When the coherence protocol needs to look up a line for any reason, this
   // is the function that should be called.
   uint64_t *cprotLookup(addr_t addr, spinlock_t *&lock, bool inv) {
     ASSERT(!SHARED); // No coherence on shared caches.

     addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
            set(tag%(1<<L2SETS));

     spin_lock(&invalidatesLock);
     if (inv) ++invalidates;
     spin_unlock(&invalidatesLock);

     spin_lock(&setLocks[set]);
     for (size_t idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
       if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {
         lock = &setLocks[set];
         return &tagarray[idx];
       }
     }

     // The directory in our protocols should ensure that execution never gets
     // here. With snooping protocols, there might be a reason to remove this
     // line and put the checks in the protocol itself. Whether this is a good
     // idea is TBD.
     ASSERT(false);
     return NULL;
   }

   void dumpSet(addr_t addr) {
     addr_t set((addr>>L2LINESZ)%(1<<L2SETS));

     for (size_t idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
       std::cerr << " 0x" << std::hex << tagarray[idx];
     }
     std::cerr << '\n';
   }

   private:
    std::vector<Cache> *peers;
    MemSysDev *lowerLevel;
    const char *name;
    int id;

    friend class REPL_T<WAYS, L2SETS, L2LINESZ>;

    std::vector<uint64_t> tagarray;
    spinlock_t setLocks[size_t(1)<<L2SETS];

    CPROT_T<L2LINESZ, Cache> *cprot;
    REPL_T<WAYS, L2SETS, L2LINESZ> repl;

    spinlock_t accessLock; // One at a time in shared LLC

    uint64_t accesses, misses, invalidates;
    spinlock_t invalidatesLock; // Some counters need locks

    void initArrays() {
      if (SHARED) spinlock_init(&accessLock);

      spinlock_init(&invalidatesLock);

      for (size_t i = 0; i < (size_t)WAYS<<L2SETS; ++i)
        tagarray[i] = 0;

      for (size_t i = 0; i < (size_t)(1<<L2SETS); ++i)
        spinlock_init(&setLocks[i]);
    }
  };

  // Group of caches at the same level. Sets up the cache peer pointers and the
  // coherence protocol.
  template
    <template<int, typename> class CPROT_T, int WAYS, int L2SETS, int L2LINESZ,
     template<int, int, int> class REPL_T>
    class CacheGrp : public MemSysDevSet
  {
   public:
    CacheGrp(int n, MemSysDev &ll, const char *name = "Unnamed") :
      cprot(caches)
    {
      for (int i = 0; i < n; ++i)
        caches.push_back(CACHE(caches, ll, i, name, &cprot));
    }

    CacheGrp(int n, MemSysDevSet &ll, const char *name = "Unnamed") :
      cprot(caches)
    {
      for (int i = 0; i < n; ++i)
        caches.push_back(CACHE(caches, ll.getMemSysDev(i), i, name, &cprot));
    }

    Cache<CPROT_T, WAYS, L2SETS, L2LINESZ, REPL_T> &getCache(size_t i) {
      return caches[i];
    }

    MemSysDev &getMemSysDev(size_t i) { return getCache(i); }

   private:
    typedef Cache<CPROT_T, WAYS, L2SETS, L2LINESZ, REPL_T> CACHE;

    std::vector<CACHE> caches;

    CPROT_T<L2LINESZ, CACHE> cprot;
  };

  // A coherence protocol for levels below L1. Takes no action to maintain
  // coherence, but does keep track of lines' modified-ness.
  template <int L2LINESZ, typename CACHE> class CPNull {
  public:
    CPNull(std::vector<CACHE> &caches) {}

    enum State {
      STATE_I = 0x00, // Invalid
      STATE_P = 0x01, // Present
      STATE_M = 0x02  // Modified
    };

    void lockAddr(addr_t addr, int id)   {}
    void unlockAddr(addr_t addr, int id) {}
    void addAddr(addr_t addr, int id) {}
    void remAddr(addr_t addr, int id) {}
    bool hitAddr(int id, addr_t addr, bool locked,
                 spinlock_t *setLock, uint64_t *line, bool wr)
    {
      if (wr) {
        *line = *line & ((~(uint64_t)0)<<L2LINESZ) | STATE_M;
      }
      spin_unlock(setLock);
      return true;
    }

    bool missAddr(int id, addr_t addr, uint64_t *line, bool wr) {
      *line = *line & ((~(uint64_t)0)<<L2LINESZ) | (wr?STATE_M:STATE_P);
      return false;
    }

    bool evAddr(int id, addr_t addr, int state) { 
      return state == STATE_M;
    }
  };
};
#endif
