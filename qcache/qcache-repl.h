#ifndef __QCACHE_REPL_H
#define __QCACHE_REPL_H

#include <map>

#include "qcache.h"
#include "qcache-bloom.h"

#include <iostream>

#define L2_EAF_SZ 12
#define EAF_HASH_FUNCS 2
#define EAF_CLEAR_INTERVAL 4

namespace Qcache {
  enum InsertionPolicy {
    INSERT_LRU, INSERT_MRU, INSERT_BIP, INSERT_DIP, INSERT_TADIP, INSERT_SHIP,
    INSERT_EAF
  };

  template <int L2SETS, int L2EAFSZ, bool PERSET> class EvictedAddressFilter {
  public:
  EvictedAddressFilter(): bf(1), acCtr(1) {
      if (PERSET) {
        bf.resize(1<<L2SETS);
        acCtr.resize(1<<L2SETS);
      }
    }

    void clear(unsigned set) {
      if (PERSET) bf[set].clear(); else bf[0].clear();
    }

    void access(unsigned set) {
      unsigned &c(acCtr[PERSET?set:0]);
      if (++c == EAF_CLEAR_INTERVAL) { c = 0; clear(set); }
    }

    bool check(unsigned set, addr_t val) {
      return bf[PERSET?set:0].check(val);
    }

    void add(unsigned set, addr_t val) {
      bf[PERSET?set:0].add(val);
    }
 
  private:
    std::vector <BloomFilter<L2EAFSZ, EAF_HASH_FUNCS, false> > bf;
    std::vector <unsigned> acCtr;
  };

  template
    <int WAYS, int L2SAMPSETS, int L2LINESZ, int L2SETS, int PSEL_BITS>
  class SetDueler {
  public:
    SetDueler(): a0(0), m0(0), a1(0), m1(0) {
      // TODO: Find a way to eliminate the repetition in the following code.

      // TODO: Do something other than silently fail when there aren't enough
      //       sets.
      if (L2SAMPSETS >= L2SETS) return;

      // Randomly sample the sets to pick some "leader" sets.
      while (sample0.size() < (1<<L2SAMPSETS)) {
        unsigned idx = rand()%(1<<L2SETS), tidx = sample0.size();
        if (sample0.find(idx) != sample0.end()) continue;
        sample0[idx] = tidx;
      }

      while (sample1.size() < (1<<L2SAMPSETS)) {
        unsigned idx = rand()%(1<<L2SETS), tidx = sample1.size();
        if (sample1.find(idx) != sample1.end() || 
            sample0.find(idx) != sample0.end()) continue;
        sample1[idx] = tidx;
      }
    }

    ~SetDueler() {
      if (a0 == 0 && a1 == 0) return;
      std::cout << "Dueler: " << a0 << ", " << m0 << ", " << a1 << ", " << m1
                << ", " << (100.0*m0)/a0 << "%, " << (100.0*m1)/a1 << "%\n";
    }

    bool inSample0(unsigned idx) {
      if (sample0.find(idx) != sample0.end()) {
        ++a0;
        return true;
      }
      return false;
    }
 
    bool inSample1(unsigned idx) {
      if (sample1.find(idx) != sample1.end()) {
        ++a1;
        return true;
      }
      return false;
    }

    void incPsel(unsigned idx) {
      ++psel[idx];
      ++m0;
      if (psel[idx] > (1<<(PSEL_BITS-1))-1) psel[idx] = (1<<(PSEL_BITS-1))-1;
    }

    void decPsel(unsigned idx) {
      --psel[idx];
      ++m1;
      if (psel[idx] < -(1<<(PSEL_BITS-1))) psel[idx] = -(1<<(PSEL_BITS-1));
    }

    bool getChoice(unsigned idx) {
      return psel[idx] > 0;
    }

  private:
    // Map from real cache index to toy cache index.
    std::map<addr_t, unsigned> sample0, sample1;

    std::map<unsigned, int> psel;
    unsigned long long a0, m0, a1, m1;
  };

  template <int WAYS, int L2SETS, int L2LINESZ, int L2SHCTSZ, 
            unsigned SHCT_BITS>
    class Shct
  {
  public:
    Shct(): reref(WAYS<<L2LINESZ), rerefSig(WAYS<<L2LINESZ), 
            wasFetch(WAYS<<L2LINESZ), c(1<<L2SHCTSZ) {}

    bool check(addr_t pc) { return c[hash(pc)] != 0; }

    void evict(size_t idx) {
      if (wasFetch[idx]) {
        wasFetch[idx] = reref[idx] = false;
        return;
      }

      unsigned shctIdx(rerefSig[idx]);
 
      if (reref[idx]) { if (c[shctIdx] != ((1<<SHCT_BITS)-1)) ++c[shctIdx]; }
      else            { if (c[shctIdx] != 0)                  --c[shctIdx]; }

      reref[idx] = false;
    }

    void insert(size_t idx, addr_t pc, bool fetch) {
      rerefSig[idx] = hash(pc);
      wasFetch[idx] = fetch;
    }

    void hit(size_t idx) {
      reref[idx] = true;
    }

  private:
    // The default hash function: repeated shift and xor
    size_t hash(addr_t pc) {
      size_t h(0);

      for (unsigned i = 0; i < 4; ++i) {
        h ^= pc&((1<<L2SHCTSZ)-1);
        pc >>= L2SHCTSZ;
      }

      return h;
    }

    std::vector<bool> reref, wasFetch;
    std::vector<unsigned> rerefSig;
    std::vector<int> c;
  };

  template
    <int WAYS, int L2SETS, int L2LINESZ, InsertionPolicy IP>
  class ReplLRUBase {
   public:
    ReplLRUBase(std::vector<addr_t> &ta) :
       tagarray(&ta[0]), tsarray(size_t(WAYS)<<L2SETS),
       tsmax(size_t(1)<<L2SETS) {}

    #define TIMESTAMP_MAX INT_MAX

    void updateRepl(addr_t set, addr_t idx, bool hit, bool wr, bool wb,
                    addr_t pc, int core, addr_t addr)
    {
      const int BIP_ALPHA = (RAND_MAX+1l)/64;

      if (hit && wb) return;

      // Handle timestamp overflows by re-numbering the lines in place. 
      if (tsmax[set] == TIMESTAMP_MAX) {
        tsmax[set] = WAYS-1;
        std::vector<bool> visited(WAYS);
        visited[idx%WAYS] = true;
        tsarray[idx] = WAYS-1;
        for (unsigned c = 0; c < WAYS-1; ++c) {
	  unsigned sIdx, mIdx;
	  timestamp_t min;
	  for (unsigned i = set*WAYS; i < (set+1)*WAYS; ++i)
	    if (!visited[i%WAYS]) { sIdx = i; break; }

	  mIdx = sIdx;
	  min = tsarray[sIdx];
 	  for (unsigned i = sIdx+1; i < (set+1)*WAYS; ++i) {
  	    if (!visited[i%WAYS] && tsarray[i] < min) {
	      min = tsarray[i]; mIdx = i;
	    }
  	  }
  	  visited[mIdx%WAYS] = true;
  	  tsarray[mIdx%WAYS] = c;
        }
      }

      tsarray[idx] = ++tsmax[set];

      bool dipChoice;
      if (IP == INSERT_DIP || IP == INSERT_TADIP) {
        unsigned idx(IP==INSERT_TADIP ? core : 0);
        // Do this lookup on every access only because we're keeping stats.
        if (dueler.inSample0(set)) {
          if (!hit) { dueler.incPsel(idx); dipChoice = false; }
	} else if (dueler.inSample1(set)) {
          if (!hit) { dueler.decPsel(idx); dipChoice = true; }
	} else {
          dipChoice = dueler.getChoice(idx);
        }
      }

      if (IP == INSERT_EAF && !hit /* ??? Clear interval in misses ??? */) {
        if (!hit)
          dipChoice = eaf.check(set, addr&~((1ll<<L2LINESZ)-1));

        eaf.access(set);
      }

      if (IP != INSERT_MRU && !hit) {
        if (IP == INSERT_BIP && rand() <= BIP_ALPHA) return;
        if ((IP == INSERT_DIP || IP == INSERT_TADIP || IP == INSERT_EAF) && 
            (!dipChoice || rand() <= BIP_ALPHA)) return;
        tsarray[idx] = -tsarray[idx];        
      }
    }

    addr_t findVictim(addr_t set) {
      size_t i = set*WAYS, minIdx = i;
      timestamp_t minTs = tsarray[i];
      if (!(tagarray[i] & ((1<<L2LINESZ)-1))) return i;
      for (i = set*WAYS + 1; i < (set+1)*WAYS; ++i) {
        if (!(tagarray[i] & ((1<<L2LINESZ)-1))) return i;
        if (tsarray[i] < minTs) { minIdx = i; minTs = tsarray[i]; }
      }

      // A valid victim has been found.
      if (IP == INSERT_EAF) eaf.add(set, tagarray[minIdx]&~((1<<L2LINESZ)-1));

      return minIdx;
    }

   private:
    typedef int timestamp_t;

    addr_t *tagarray;
    std::vector<timestamp_t> tsarray, tsmax;

    SetDueler<WAYS, 5, L2LINESZ, L2SETS, 5> dueler;

    EvictedAddressFilter<L2SETS, L2_EAF_SZ, true> eaf;
    //BloomFilter<L2_EAF_SZ, EAF_HASH_FUNCS, IP == INSERT_EAF> eaf;
  };

  // I'm sorry. C++11 alias templates soon, but for compatibility, here's this:
  #define QCACHE_REPL_PASSTHROUGH_FUNCS \
    void updateRepl(addr_t s, addr_t i, bool h, bool w, bool wb, addr_t pc, \
                    int c, addr_t a)	\
    { \
      r.updateRepl(s, i, h, w, wb, pc, c, a);	\
    } \
    addr_t findVictim(addr_t set) { return r.findVictim(set); }

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU {
  public:
    ReplLRU(std::vector<addr_t> &ta):  r(ta) { }

    QCACHE_REPL_PASSTHROUGH_FUNCS
    
  private:
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_MRU> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_LIP {
  public:
    ReplLRU_LIP(std::vector<addr_t> &ta):  r(ta) { }

    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_LRU> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_BIP {
   public:
     ReplLRU_BIP(std::vector<addr_t> &ta):  r(ta) { }

     QCACHE_REPL_PASSTHROUGH_FUNCS

   private:
     ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_BIP> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_DIP {
  public:
    ReplLRU_DIP(std::vector<addr_t> &ta):
      r(ta), tagarray(&ta[0]) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_DIP> r;
    addr_t *tagarray;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_TADIP {
  public:
    ReplLRU_TADIP(std::vector<addr_t> &ta):
      r(ta), tagarray(&ta[0]) {}
 
    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_TADIP> r;
    addr_t *tagarray;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_EAF {
  public:
    ReplLRU_EAF(std::vector<addr_t> &ta):
      r(ta), tagarray(&ta[0]) {}
  
    QCACHE_REPL_PASSTHROUGH_FUNCS
  
  private:
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_EAF> r;
    addr_t *tagarray;
  };

  #define RRIP_MAX_STATE 3

  template <int WAYS, int L2SETS, int L2LINESZ, InsertionPolicy IP>
    class ReplRRIPBase
  {
  public:
    ReplRRIPBase(std::vector<addr_t> &ta) :
      tagarray(&ta[0]), ctr(size_t(WAYS)<<L2SETS) {}

    void updateRepl(addr_t set, addr_t idx, bool h, bool w, bool wb,
                    addr_t pc, int core, addr_t addr)
    {
      const int BRRIP_ALPHA = (RAND_MAX+1l)/64;

      bool isFetch((pc&~((1ull<<L2LINESZ)-1))==(addr&~((1ull<<L2LINESZ)-1)));

      bool drripChoice;
      if (IP == INSERT_DIP || IP == INSERT_TADIP) {
        unsigned idx(IP==INSERT_TADIP ? core : 0);

        // Do this lookup on every access only because we're keeping stats.
        if (dueler.inSample0(set)) {
          if (!h) { dueler.incPsel(idx); drripChoice = false; }
        } else if (dueler.inSample1(set)) {
          if (!h) { dueler.decPsel(idx); drripChoice = true; }
        } else {
          drripChoice = dueler.getChoice(idx);
        }
      }

      if (IP == INSERT_EAF && !h /*??? (see comment in LRU) ???*/ ) {
        if (!h)
          drripChoice = eaf.check(set, addr&~((1ll<<L2LINESZ)-1));

        eaf.access(set);
      }

      if (IP == INSERT_SHIP) {
        if (h) shct.hit(idx);
        else shct.insert(idx, pc, isFetch);
      }

      if (h) {
        if (!wb) ctr[idx] = 0;
      } else {
        if (IP==INSERT_LRU ||
            IP==INSERT_SHIP && !isFetch && shct.check(pc) ||
            (IP==INSERT_BIP ||
             ((IP==INSERT_DIP || IP==INSERT_TADIP || IP==INSERT_EAF)
              && drripChoice))
                && rand() > BRRIP_ALPHA)
	{
          ctr[idx] = RRIP_MAX_STATE - 1;
        } else {
          ctr[idx] = RRIP_MAX_STATE - 2;
        }
      }
    }

    addr_t findVictim(addr_t set) {
      unsigned vCt = 0, vC[WAYS]; // Victim candidates

      // Look for an invalid line.
      for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i)
        if (!(tagarray[i] & ((1<<L2LINESZ)-1))) return i;

      while (vCt == 0) {
        // Look for counters in max state
        for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i)
          if (ctr[i] == RRIP_MAX_STATE) vC[vCt++] = i;

        if (vCt > 0) break;

        // Increment all of the counters
        for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i) {
          ASSERT(ctr[i] < RRIP_MAX_STATE);
          ++ctr[i];
        }
      }

      // Random to break ties.
      int vic;
      do { vic = rand(); } while (vic > RAND_MAX/vCt*vCt);
      vic %= vCt;

      unsigned victim = vC[vic];

      if (IP == INSERT_EAF)
        eaf.add(set, tagarray[victim]&~((1ll<<L2LINESZ)-1));

      else if (IP == INSERT_SHIP) shct.evict(victim);

      return victim;
    }
    
  private:
    addr_t *tagarray;

    std::vector<unsigned char> ctr;

    SetDueler<WAYS, 5, L2LINESZ, L2SETS, 5> dueler;

    EvictedAddressFilter<L2SETS, L2_EAF_SZ, true> eaf;
    //BloomFilter<L2_EAF_SZ, EAF_HASH_FUNCS, IP == INSERT_EAF> eaf;

    Shct<WAYS, L2LINESZ, L2SETS, 14, 2> shct;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplSRRIP {
  public:
    ReplSRRIP(std::vector<addr_t> &ta):  r(ta) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplRRIPBase<WAYS, L2SETS, L2LINESZ, INSERT_MRU> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplBRRIP {
  public:
    ReplBRRIP(std::vector<addr_t> &ta):  r(ta) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplRRIPBase<WAYS, L2SETS, L2LINESZ, INSERT_BIP> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplDRRIP {
  public:
    ReplDRRIP(std::vector<addr_t> &ta): r(ta) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplRRIPBase<WAYS, L2SETS, L2LINESZ, INSERT_DIP> r;  
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplTADRRIP {
  public:
  ReplTADRRIP(std::vector<addr_t> &ta): r(ta) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

      private:
    ReplRRIPBase<WAYS, L2SETS, L2LINESZ, INSERT_TADIP> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplSHIP {
  public:
  ReplSHIP(std::vector<addr_t> &ta): r(ta) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

      private:
    ReplRRIPBase<WAYS, L2SETS, L2LINESZ, INSERT_SHIP> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplERRIP {
  public:
    ReplERRIP(std::vector<addr_t> &ta): r(ta) {}

    QCACHE_REPL_PASSTHROUGH_FUNCS

      private:
    ReplRRIPBase<WAYS, L2SETS, L2LINESZ, INSERT_EAF> r;
  };
};

#endif
