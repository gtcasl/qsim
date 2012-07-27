#ifndef __QCACHE_H
#define __QCACHE_H

#include <iostream>
#include <iomanip>

#include <vector>
#include <map>
#include <set>
#include <queue>

#include <stdint.h>
#include <pthread.h>
#include <limits.h>

#include <stdlib.h>

#include <typeinfo>

//#define DEBUG
//#define ENABLE_ASSERTIONS

#ifdef ENABLE_ASSERTIONS
#define ASSERT(b) do { if (!(b)) { \
  std::cerr << "Error: Failed assertion at " << __FILE__ << ':' << std::dec \
            << __LINE__ << '\n'; \
  abort(); \
} } while(0)

#else
#define ASSERT(b) do {} while(0)
#endif

#define MEM_BARRIER() do { __asm__ __volatile__ ("mfence":::"memory");} while(0)

namespace Qcache {
  extern pthread_mutex_t errLock;
  extern bool printResults;

  const unsigned DIR_BANKS = 256; // # coherence directory banks

  typedef pthread_spinlock_t spinlock_t;
  #define spinlock_init(s) do { pthread_spin_init((s), 0); } while (0)
  #define spin_lock(s) do { pthread_spin_lock((s)); } while (0)
  #define spin_unlock(s) do { pthread_spin_unlock((s)); } while (0)
  typedef uint64_t addr_t;

  // Every level in the memory hierarchy is one of these.
  class MemSysDev {
   public:
    ~MemSysDev() {}

    virtual int getLatency() { ASSERT(false); }

    virtual int access(addr_t addr, addr_t pc, int core, int wr,
                       bool *flagptr=NULL, addr_t** lp=NULL)
    {
      ASSERT(false);
    }

    virtual void invalidate(addr_t addr) { ASSERT(false); }

    virtual uint64_t *cprotLookup(addr_t addr, spinlock_t *&l, bool inv,
                                  bool lock=true)
    {
      ASSERT(false);
    }

    virtual void l1LockAddr(addr_t addr) { ASSERT(false); }
    virtual void l1UnlockAddr(addr_t addr) { ASSERT(false); }
    virtual void l1EvictAddr(addr_t addr) { ASSERT(false); }


    virtual void setUpperLevel(MemSysDev *) {}

    virtual bool isShared() { return true; }

    enum AccType {
      READ = 0,
      WRITE = 1,
      WRITEBACK = 2
    };
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
    Tracer(std::ostream &tf, int delay=50) : tracefile(tf), delay(delay) {}

    int access(addr_t addr, addr_t pc, int core, int wr, bool *flagptr = NULL,
               addr_t **lp = NULL)
    {
      tracefile << std::dec << addr << (wr?" W\n":" R\n");
      return delay;
    }

    int getLatency() { return delay; }

   private:
    std::ostream &tracefile;
    int delay;
  };

  // Cycle-accurate DRAM is expensive and complex, so here's a fast functional
  // model with configurable latency.
  template <int MISS_LAT, int HIT_LAT, int QLEN, typename DIM_T,
            template<typename> class ADDRMAP_T>
    class FuncDram : public MemSysDev
  {
  public:
    FuncDram():
      banks(m.d.channels() * m.d.ranks() * m.d.banks()),
      prevRow(banks.size(), -1), accesses(0), hits(0)
    {
      pthread_mutex_init(&lock, NULL);
    }

    ~FuncDram() {
      if (!printResults) return;
      std::cout << "FuncDram: " << accesses << ", " << hits << '\n';
    }

    int access(addr_t addr, addr_t pc, int core, int wr, bool *flagptr=NULL,
               addr_t **lp = NULL)
    {
      pthread_mutex_lock(&lock);

      unsigned bankIdx(getBankIdx(addr)), row(m.getRow(addr));
      bool rowHit(banks[bankIdx].find(row) != banks[bankIdx].end() ||
                  prevRow[bankIdx] != -1 && prevRow[bankIdx] == row);
      banks[bankIdx].insert(row);
      prevRow[bankIdx] = row;

      ASSERT(bankIdx < banks.size());

      ++accesses; if (rowHit) ++hits;

      accQ.push(addr);
      if (accQ.size() > QLEN) {
        addr_t removeMe(accQ.front()); accQ.pop();
        unsigned bankIdx(getBankIdx(removeMe)), row(m.getRow(removeMe));
        ASSERT(banks[bankIdx].find(row) != banks[bankIdx].end());
        banks[bankIdx].erase(banks[bankIdx].find(row));
        ASSERT(banks[bankIdx].size() <= QLEN);
      }

      pthread_mutex_unlock(&lock);

      return rowHit?HIT_LAT:MISS_LAT;
    }

    int getLatency() { return MISS_LAT; }

  private:
    unsigned getBankIdx(addr_t a) {
      return m.d.banks()*(m.d.ranks()*m.getChannel(a) + m.getRank(a)) 
             + m.getBank(a);
    }

    ADDRMAP_T<DIM_T> m;

    std::vector<std::multiset<addr_t> > banks;
    std::vector<int> prevRow;
    std::queue<addr_t> accQ;

    uint64_t accesses, hits;

    pthread_mutex_t lock;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplRand {
  public:
    ReplRand(std::vector<addr_t> &ta): tagarray(&ta[0]) {}

    void updateRepl(addr_t set, addr_t idx, bool hit, bool wr, bool wb,
                    addr_t pc, int core) {}

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
    <int LATENCY, template<int, typename> class CPROT_T,
    int WAYS, int L2SETS, int L2LINESZ, template<int, int, int> class REPL_T,
    bool SHARED=false>
  class Cache : public MemSysDev
  {
   public:
     Cache(MemSysDev &ll, const char *n = "Unnamed",
           CPROT_T<L2LINESZ, Cache> *cp = NULL) :
      tagarray(size_t(WAYS)<<L2SETS), repl(tagarray),
      peers(NULL), lowerLevel(&ll), upperLevel(NULL),
      cprot(cp), id(0), name(n),
      accesses(0), misses(0), invalidates(0), writebacks(0)
    {
      initArrays();
      if (!lowerLevel->isShared()) lowerLevel->setUpperLevel(this);
    }

    Cache(std::vector<Cache> &peers, MemSysDev &ll, int id,
          const char *n = "Unnamed", CPROT_T<L2LINESZ, Cache> *cp = NULL) :
      tagarray(WAYS<<L2SETS), repl(tagarray),
      peers(&peers), lowerLevel(&ll), upperLevel(NULL),
      cprot(cp), id(id), name(n),
      accesses(0), misses(0), invalidates(0), writebacks(0)
    {
      initArrays();
      if (!lowerLevel->isShared()) lowerLevel->setUpperLevel(this);
    }

    Cache(const Cache &c):
      tagarray(c.tagarray), repl(tagarray), peers(c.peers), cprot(c.cprot),
      id(c.id), name(c.name), accesses(c.accesses), misses(c.misses),
      invalidates(c.invalidates), writebacks(c.writebacks),
      lowerLevel(c.lowerLevel), 
      upperLevel(c.upperLevel)
    {
      if (!lowerLevel->isShared()) {
        lowerLevel->setUpperLevel(this);
      }
      initArrays();
    }

    ~Cache() {
      if (!printResults) return;
      std::cout << name << ", " << id << ", " << accesses << ", " << misses 
                << ", " << writebacks << ", " << invalidates << ", "
                << (100.0*misses)/accesses << "%\n";
    }

    int getLatency() { return LATENCY + lowerLevel->getLatency(); }

    void l1LockAddr(addr_t addr) {
      if (!upperLevel) cprot->lockAddr(addr, id);
      else upperLevel->l1LockAddr(addr);
    }

    void l1UnlockAddr(addr_t addr) {
      if (!upperLevel) cprot->unlockAddr(addr, id);
      else upperLevel->l1UnlockAddr(addr);
    }

    void l1EvictAddr(addr_t addr) {
      if (!upperLevel) cprot->evAddr(id, addr);
      else upperLevel->l1EvictAddr(addr);
    }

    int access(addr_t addr, addr_t pc, int core, int wr,
               bool *flagptr=NULL,  addr_t **lineptr=NULL)
    {
      bool hit = false;
      int lat = 0;

      addr_t *llLineptr;
      bool wbState = false;

      // Writebacks are not included in the miss or access count.
      if (SHARED) spin_lock(&accessLock);

      if (!wr) ++accesses;
      if (wr == WRITEBACK) ++writebacks;

      addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
             set(tag%(1<<L2SETS));

      addr &= ~stateMask; // Throw away address LSBs.

      spin_lock(&setLocks[set]);
      addr_t idx;
      for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
        if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {
          repl.updateRepl(set, idx, true, wr, wr==WRITEBACK, pc, core);
          // Check with the coherence protocol-- we can still be invalidated!
          hit = cprot->hitAddr(id, addr, false, &setLocks[set], &tagarray[idx],
                               wr);
          if (!hit) { spin_lock(&setLocks[set]); break; }
          else if (lineptr) *lineptr = &tagarray[idx];
        }
      }

      if (!hit) {
        if (!wr) ++misses;

        size_t vidx(repl.findVictim(set));
        addr_t victimAddr(tagarray[vidx]&~stateMask);
        int victimState(tagarray[vidx]&stateMask);
        uint64_t victimVal(tagarray[vidx]);

        // Lock the victim address if this is the LLPC and there is a valid
        // line to evict.
        bool victimLocked(lowerLevel->isShared() && victimState);

        ASSERT(vidx >= set*WAYS && vidx < (set+1)*WAYS);
        ASSERT(victimState == 0 || victimAddr != addr);
        
        spin_unlock(&setLocks[set]);

        if (victimLocked) l1LockAddr(victimAddr);
        spin_lock(&setLocks[set]);

	// The victim state may have changed:
        if ((tagarray[vidx]&stateMask) != victimState) {
          // Nothing should ever cause the address of the victim to change.
          ASSERT(victimAddr == (tagarray[vidx]&~stateMask));

          // Update victim state.
          victimState = (tagarray[vidx]&stateMask);
        }

        spin_unlock(&setLocks[set]);

        if (victimState) {
          // Now it's "always write back evicted lines" to ensure the line is
          // in the lower level private cache. Strictly inclusive caches would
          // be a way to avoid this.  
          bool doWriteback;
          if (lowerLevel->isShared()) {
            doWriteback = cprot->dirty(victimState);
          } else {
            doWriteback = wbState = true;
          }

          if (doWriteback && lowerLevel) {
            lowerLevel->access(victimAddr, pc, core, WRITEBACK, 0, &llLineptr);
          }
        }

        if (victimLocked) {
          l1EvictAddr(victimAddr);
          l1UnlockAddr(victimAddr);
        }
        
        // Lock access block.
        cprot->lockAddr(addr, id);
        spin_lock(&setLocks[set]);

        // An invalid victim shouldn't have magically become valid.
        ASSERT(victimState || !(tagarray[vidx] & stateMask));

        if (wbState) {
          *llLineptr &= ~stateMask;
          *llLineptr |= tagarray[vidx] & stateMask;
        }

        tagarray[vidx] = addr|0x01;
        repl.updateRepl(set, vidx, false, wr, wr==WRITEBACK, pc, core);
        spin_unlock(&setLocks[set]);

        if (!cprot->missAddr(id, addr, &tagarray[vidx], wr) && wr != WRITEBACK)
        {
          lat = lowerLevel->access(
            tag<<L2LINESZ, pc, core, READ, flagptr, &llLineptr
          );

          if (!lowerLevel->isShared()) {
            tagarray[vidx] &= ~stateMask;
            tagarray[vidx] |= (*llLineptr & stateMask);
          }
        }
        
        if (!lowerLevel->isShared()) lowerLevel->invalidate(tag<<L2LINESZ);

        if (lineptr) *lineptr = &tagarray[vidx];

        // Unlock access block.
        cprot->unlockAddr(addr, id);
      }

      if (SHARED) spin_unlock(&accessLock);

      return lat + LATENCY;
    }

    void invalidate(addr_t addr) {
      ASSERT(!SHARED);

      addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
             set(tag%(1<<L2SETS));

      spin_lock(&setLocks[set]);
      addr_t idx;
      for (idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
        if ((tagarray[idx]>>L2LINESZ)==tag) {
          tagarray[idx] &= ~((1l<<L2LINESZ)-1);
          // No need to get invalidates lock since we are not shared and are
          // not accessed by the coherence protocol.
          ++invalidates;
        }
      }
      spin_unlock(&setLocks[set]);
    }

    bool isShared() { return SHARED; }

   // When the coherence protocol needs to look up a line for any reason, this
   // is the function that should be called.
   uint64_t *cprotLookup(addr_t addr, spinlock_t *&lock, bool inv, bool l=true) {
     ASSERT(!SHARED); // No coherence on shared caches.

     addr_t stateMask((1<<L2LINESZ)-1), tag(addr>>L2LINESZ),
            set(tag%(1<<L2SETS));

     lock = &setLocks[set];
     if (l) spin_lock(&setLocks[set]);
     for (size_t idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
       if ((tagarray[idx]>>L2LINESZ)==tag && (tagarray[idx]&stateMask)) {

	 spin_lock(&invalidatesLock);
	 if (inv) ++invalidates;
	 spin_unlock(&invalidatesLock);

         return &tagarray[idx];
       }
     }

     // The line may be in lower-level private caches.
     spinlock_t *tmpLock;
     uint64_t *rval = lowerLevel->cprotLookup(addr, tmpLock, inv, false);
     //spin_unlock(tmpLock);

     return rval;
   }

   void dumpSet(addr_t addr) {
     addr_t set((addr>>L2LINESZ)%(1<<L2SETS));

     for (size_t idx = set*WAYS; idx < (set+1)*WAYS; ++idx) {
       std::cerr << " 0x" << std::hex << tagarray[idx];
     }
     std::cerr << '\n';
   }

   void setUpperLevel(MemSysDev *uL) { upperLevel = uL; }

   private:
    std::vector<Cache> *peers;
    MemSysDev *lowerLevel, *upperLevel;
    const char *name;
    int id;

    friend class REPL_T<WAYS, L2SETS, L2LINESZ>;
    friend class CPROT_T<L2LINESZ, Cache>;

    std::vector<uint64_t> tagarray;
    spinlock_t setLocks[size_t(1)<<L2SETS];

    CPROT_T<L2LINESZ, Cache> *cprot;
    REPL_T<WAYS, L2SETS, L2LINESZ> repl;

    spinlock_t accessLock; // One at a time in shared LLC

    uint64_t accesses, misses, invalidates, writebacks;
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
    <int LATENCY, template<int, typename> class CPROT_T,
     int WAYS, int L2SETS, int L2LINESZ, template<int, int, int> class REPL_T>
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

    Cache<LATENCY, CPROT_T, WAYS, L2SETS, L2LINESZ, REPL_T> &getCache(size_t i)
    {
      return caches[i];
    }

    MemSysDev &getMemSysDev(size_t i) { return getCache(i); }

   private:
    typedef Cache<LATENCY, CPROT_T, WAYS, L2SETS, L2LINESZ, REPL_T> CACHE;

    std::vector<CACHE> caches;

    CPROT_T<L2LINESZ, CACHE> cprot;
  };

  // A coherence protocol for levels below L1. Takes no action to maintain
  // coherence, but does keep track of lines' modified-ness.
  template <int L2LINESZ, typename CACHE> class CPNull {
  public:
    CPNull(std::vector<CACHE> &caches) {}

    enum State {
      // The values given here make present and modified line up with shared
      // and modified in M[[O]E]SI, respectively. This enables noncoherent
      // reading of lines containing code.
      STATE_I = 0x00, // Invalid
      STATE_M = 0x02, // Present
      STATE_P = 0x05  // Modified
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

    void evAddr(int id, addr_t addr) {}

    bool dirty(int state) {
      return state == STATE_M;
    }
  };
};
#endif
