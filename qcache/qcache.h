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

#if 1
#define ASSERT(b) do { if (!(b)) { \
  std::cerr << "Error: Failed assertion at " << __FILE__ << ':' << std::dec \
            << __LINE__ << '\n'; \
  abort(); \
} } while(0)

#else
#define ASSERT(b) do {} while(0)
#endif

namespace Qcache {
  const unsigned DIR_BANKS = 256; // # coherence directory banks

  typedef unsigned timestamp_t;
  #define TIMESTAMP_MAX UINT_MAX

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

    virtual void access(addr_t addr, bool wr) = 0;
    virtual void invalidate(addr_t addr) = 0;
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

    void access(addr_t addr, bool wr) {
      tracefile << std::dec << addr << (wr?" W\n":" R\n");
    }

    void invalidate(addr_t addr) { throw InvalidAccess(); }
   private:
    std::ostream &tracefile;
  };

  // Caches, private or shared, of any dimension
  template
    <template<int, typename> class CPROT_T,
     int WAYS, int L2SETS, int L2LINESZ, bool SHARED=false>
  class Cache : public MemSysDev
  {
   public:
     Cache(MemSysDev &ll, const char *n = "Unnamed",
           CPROT_T<L2LINESZ, Cache> *cp = NULL) :
      peers(NULL), lowerLevel(&ll), cprot(cp), id(0), name(n),
      accesses(0), misses(0), invalidates(0)
    {
      initArrays();
    }

    Cache(std::vector<Cache> &peers, MemSysDev &ll, int id,
          const char *n = "Unnamed", CPROT_T<L2LINESZ, Cache> *cp = NULL) :
      peers(&peers), lowerLevel(&ll), cprot(cp), id(id), name(n),
      accesses(0), misses(0), invalidates(0)
    {
      initArrays();
    }

    ~Cache() {
      if (accesses == 0) return;
      std::cout << name << ", " << id << ", " << accesses << ", " << misses 
                << ", " << invalidates << '\n';
    }

    void access(addr_t addr, bool wr) {
      ++accesses;
      if (SHARED) spin_lock(&accessLock);

      addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
             set(tag%(1<<L2SETS));

      addr &= ~stateMask; // Throw away address LSBs.

      cprot->lockAddr(addr, id); // TODO: XX: Only lock when necessary!
      spin_lock(&setLocks[set]);
      addr_t idx;
      for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
        if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {
          updateRepl(set, idx);
          spin_unlock(&setLocks[set]);
          cprot->hitAddr(id, addr, &tagarray[idx], wr);
          cprot->unlockAddr(addr, id); // XX
          goto finish;
        }
      }
      cprot->unlockAddr(addr, id); // XX
      ++misses;

      size_t vidx;
      addr_t victimAddr;
      int victimState;
      for (;;) {
        vidx = findVictim(set);
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
            updateRepl(set, idx);
            spin_unlock(&setLocks[set]);
            cprot->hitAddr(id, addr, &tagarray[idx], wr);
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

        if (victimState && (tagarray[vidx] & ~stateMask) != victimAddr) {
          // Victim has changed in the array.
          spin_unlock(&setLocks[set]);
          if (victimAddr > addr) cprot->unlockAddr(victimAddr, id);
          cprot->unlockAddr(addr, id);
          if (victimAddr < addr) cprot->unlockAddr(victimAddr, id);
          spin_lock(&setLocks[set]);
          continue;
	}
        
        break;
      }
      tagarray[vidx] = addr|0x01;
      updateRepl(set, vidx); // MRU insertion policy.
      spin_unlock(&setLocks[set]);
      cprot->evAddr(id, victimAddr);
      if (!cprot->missAddr(id, addr, &tagarray[vidx], wr))
        lowerLevel->access(tag<<L2LINESZ, wr);

    missFinish:
      // Unlock access block and victim block in reverse order.
      if (victimState && victimAddr > addr) cprot->unlockAddr(victimAddr, id);
      cprot->unlockAddr(addr, id);
      if (victimState && victimAddr < addr) cprot->unlockAddr(victimAddr, id);

    finish:
      if (SHARED) spin_unlock(&accessLock);
    }

    void invalidate(addr_t addr) {
      if (SHARED) spin_lock(&accessLock);

      ASSERT(false); // TODO: Make this function coherence-compatible

      spin_lock(&invalidatesLock);
      ++invalidates;
      spin_unlock(&invalidatesLock);

      addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
             set(tag%(1<<L2SETS));

      spin_lock(&setLocks[set]);
      addr_t idx;
      for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
        if ((tagarray[idx]>>L2LINESZ)==tag) tagarray[idx] = 0;
      }
      spin_unlock(&setLocks[set]);

      if (SHARED) spin_unlock(&accessLock);
    }

   // When the coherence protocol needs to look up a line for any reason, this
   // is the function that should be called.
   uint64_t *cprotLookup(addr_t addr, int state) {
     ASSERT(!SHARED); // No coherence on shared caches.

     addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
            set(tag%(1<<L2SETS));

     if (state == 0) {
       spin_lock(&invalidatesLock);
       ++invalidates;
       spin_unlock(&invalidatesLock);
     }

     for (size_t idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
       if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {
         tagarray[idx] = tagarray[idx] & ((~(uint64_t)0)<<L2LINESZ)|state;
         spin_unlock(&setLocks[set]);
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

   private:
    std::vector<Cache> *peers;
    MemSysDev *lowerLevel;
    const char *name;
    int id;
    CPROT_T<L2LINESZ, Cache> *cprot;

    uint64_t tagarray[(size_t)WAYS<<L2SETS];
    timestamp_t tsarray[(size_t)WAYS<<L2SETS];
    timestamp_t tsmax[1l<<L2SETS];
    spinlock_t setLocks[1l<<L2SETS];

    spinlock_t accessLock; // One at a time in shared LLC

    uint64_t accesses, misses, invalidates;
    spinlock_t invalidatesLock; // Some counters need locks

    void updateRepl(addr_t set, addr_t idx) {
      // TODO: Handle timestamp overflow.
      ASSERT(tsmax[set] != TIMESTAMP_MAX);
      tsarray[idx] = ++tsmax[set];
    }

    addr_t findVictim(addr_t set) {
      size_t i = set*WAYS, maxIdx = i;
      timestamp_t maxTs = tsarray[i];
      for (i = set*WAYS + 1; i < (set+1)*WAYS; ++i) {
        if (!(tagarray[i] & ((1<<L2LINESZ)-1))) return i;
        if (tsarray[i] > maxTs) { maxIdx = i; maxTs = tsarray[i]; }
      }
      return maxIdx;
    }

    void initArrays() {
      if (SHARED) spinlock_init(&accessLock);

      spinlock_init(&invalidatesLock);

      for (size_t i = 0; i < (size_t)WAYS<<L2SETS; ++i) {
        tsarray[i] = tagarray[i] = 0;
      }

      for (size_t i = 0; i < (size_t)(1<<L2SETS); ++i) {
        tsmax[i] = 0;
        spinlock_init(&setLocks[i]);
      }
    }
  };

  // Group of caches at the same level. Sets up the cache peer pointers and the
  // coherence protocol.
  template
    <template<int, typename> class CPROT_T, int WAYS, int L2SETS, int L2LINESZ>
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

    Cache<CPROT_T, WAYS, L2SETS, L2LINESZ> &getCache(size_t i) {
      return caches[i];
    }

    MemSysDev &getMemSysDev(size_t i) { return getCache(i); }

   private:
    typedef Cache<CPROT_T, WAYS, L2SETS, L2LINESZ> CACHE;

    std::vector<CACHE> caches;

    CPROT_T<L2LINESZ, CACHE> cprot;
  };

  // A coherence protocol for levels below L1. Takes no action to maintain
  // coherence.
  template <int L2LINESZ, typename CACHE> class CPNull {
  public:
    CPNull(std::vector<CACHE> &caches) {}

    void lockAddr(addr_t addr, int id)   {}
    void unlockAddr(addr_t addr, int id) {}
    void addAddr(addr_t addr, int id) {}
    void remAddr(addr_t addr, int id) {}
    void hitAddr(int id, addr_t addr, uint64_t *line, bool wr) {}
    bool missAddr(int id, addr_t addr, uint64_t *line, bool wr) {return false;}
    void evAddr(int id, addr_t addr) {}
  };

  // Directory for directory-based protocols.
  template <int L2LINESZ> class CoherenceDir {
  public:
    void lockAddr(addr_t addr, int id) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      spin_lock(&banks[getBankIdx(addr)].getEntry(addr).lock);
      banks[getBankIdx(addr)].getEntry(addr).lockHolder = id;
    }

    void unlockAddr(addr_t addr, int id) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      spin_unlock(&banks[getBankIdx(addr)].getEntry(addr).lock);
      banks[getBankIdx(addr)].getEntry(addr).lockHolder = -1;
    }

    void addAddr(addr_t addr, int id) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      banks[getBankIdx(addr)].getEntry(addr).present.insert(id);
    }

    void remAddr(addr_t addr, int id) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      banks[getBankIdx(addr)].getEntry(addr).present.erase(id);
    }

    std::set<int>::iterator idsBegin(addr_t addr, int id) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      ASSERT(banks[getBankIdx(addr)].getEntry(addr).lockHolder == id);
      return banks[getBankIdx(addr)].getEntry(addr).present.begin();
    }

    std::set<int>::iterator idsEnd(addr_t addr, int id) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      ASSERT(banks[getBankIdx(addr)].getEntry(addr).lockHolder == id);
      return banks[getBankIdx(addr)].getEntry(addr).present.end();
    }

    std::set<int>::iterator clearIds(addr_t addr, int remaining) {
      ASSERT(addr%(1<<L2LINESZ) == 0);
      ASSERT(banks[getBankIdx(addr)].getEntry(addr).lockHolder == remaining);
      std::set<int> &p(banks[getBankIdx(addr)].getEntry(addr).present);
      p.clear();
      p.insert(remaining);
    }

  private:
    struct Entry {
      Entry(): present() { spinlock_init(&lock); }
      spinlock_t lock;
      int lockHolder;
      std::set<int> present;
    };

    struct Bank {
      Bank(): entries() { spinlock_init(&lock); }

      // We want to use a map<addr_t, Entry> since the address space will tend
      // to be fragmented, but we want it to be thread safe. This code ensures
      // that.
      Entry &getEntry(addr_t addr) {
        Entry *rval;
        spin_lock(&lock);
        rval = entries[addr];
        if (!rval) entries[addr] = rval = new Entry();
        spin_unlock(&lock);

        return *rval;
      }

      spinlock_t lock;
      std::map<addr_t, Entry*> entries;
    };

    size_t getBankIdx(addr_t addr) {
      return (addr>>L2LINESZ)%DIR_BANKS;
    }

    Bank banks[DIR_BANKS];
  };

  // Directory MSI coherence protocol.
  template <int L2LINESZ, typename CACHE> class CPDirMsi {
  public:
    CPDirMsi(std::vector<CACHE> &caches): caches(caches) {}

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

    void hitAddr(int id, addr_t addr, uint64_t *line, bool wr) {
      if (getState(line) == STATE_M) {
        return;
      } else if (getState(line) == STATE_S) {
        if (!wr) return;
        // Invalidate all of the remote lines.
	for (std::set<int>::iterator it = dir.idsBegin(addr, id);
	     it != dir.idsEnd(addr, id); ++it)
	{
          if (*it == id) continue;
          caches[*it].cprotLookup(addr, STATE_I);
        }
        dir.clearIds(addr, id);
      } else {
	std::cerr << "Invalid state: " << getState(line) << '\n';
        ASSERT(false); // Invalid state.
      }
    }

    bool missAddr(int id, addr_t addr, uint64_t *line, bool wr) {
      setState(line, wr?STATE_M:STATE_S);
      addAddr(addr, id);

      if (wr) {
        // Invalidate all of the remote lines.
        for (std::set<int>::iterator it = dir.idsBegin(addr, id);
             it != dir.idsEnd(addr, id); ++it)
	{
          if (*it == id) continue;
          caches[*it].cprotLookup(addr, STATE_I);
	}
        dir.clearIds(addr, id);
      }

      return false;
    }

    void evAddr(int id, addr_t addr) {
      remAddr(addr, id);
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
