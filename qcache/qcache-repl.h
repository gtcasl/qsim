#ifndef __QCACHE_REPL_H
#define __QCACHE_REPL_H

#include <map>

#include "qcache.h"

namespace Qcache {
  enum InsertionPolicy {
    INSERT_LRU, INSERT_MRU, INSERT_BIP, INSERT_DIP
  };

  template
    <int WAYS, int L2SAMPSETS, int L2LINESZ, int L2SETS, int PSEL_BITS>
  class SetDueler {
  public:
    SetDueler(): psel(0), a0(0), m0(0), a1(0), m1(0) {
      // TODO: Find a way to eliminate the repetition in the following code.

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

    void incPsel() {
      ++psel;
      ++m0;
      if (psel > (1<<(PSEL_BITS-1))-1) psel = (1<<(PSEL_BITS-1))-1;
    }

    void decPsel() {
      --psel;
      ++m1;
      if (psel < -(1<<(PSEL_BITS-1))) psel = -(1<<(PSEL_BITS-1));
    }

    bool getChoice() {
      return psel > 0;
    }

  private:
    // Map from real cache index to toy cache index.
    std::map<addr_t, unsigned> sample0, sample1;

    int psel;
    unsigned long long a0, m0, a1, m1;
  };

  template
    <int WAYS, int L2SETS, int L2LINESZ, InsertionPolicy IP>
  class ReplLRUBase {
   public:
  ReplLRUBase(std::vector<addr_t> &ta) :
       tagarray(&ta[0]), tsarray(size_t(WAYS)<<L2SETS),
       tsmax(size_t(1)<<L2SETS) {}

    #define TIMESTAMP_MAX INT_MAX

    void updateRepl(addr_t set, addr_t idx, bool hit, bool wr) {
      const int BIP_ALPHA = (RAND_MAX+1l)/64;

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
      if (IP == INSERT_DIP) {
        // Do this lookup on every access only because we're keeping stats.
        if (dueler.inSample0(set)) {
          if (!hit) { dueler.incPsel(); dipChoice = false; }
	} else if (dueler.inSample1(set)) {
          if (!hit) { dueler.decPsel(); dipChoice = true; }
	} else {
          dipChoice = dueler.getChoice();
        }
      }

      if (IP != INSERT_MRU && !hit) {
        if (IP == INSERT_BIP && rand() <= BIP_ALPHA) return;
        if (IP == INSERT_DIP && (!dipChoice || rand() <= BIP_ALPHA)) return;
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
      return minIdx;
    }

   private:
    typedef int timestamp_t;

    addr_t *tagarray;
    std::vector<timestamp_t> tsarray, tsmax;
    SetDueler<WAYS, 5, L2LINESZ, L2SETS, 5> dueler;
  };

  // I'm sorry. C++11 alias templates soon, but for compatibility, here's this:
  #define QCACHE_REPL_PASSTHROUGH_FUNCS \
    void updateRepl(addr_t s, addr_t i, bool h, bool w) { \
      r.updateRepl(s, i, h, w); \
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

  #define RRIP_MAX_STATE 3

  template <int WAYS, int L2SETS, int L2LINESZ, InsertionPolicy IP>
    class ReplRRIPBase
  {
  public:
    ReplRRIPBase(std::vector<addr_t> &ta) :
      tagarray(&ta[0]), ctr(size_t(WAYS)<<L2SETS) {}

    void updateRepl(addr_t set, addr_t idx, bool h, bool w) {
      const int BRRIP_ALPHA = (RAND_MAX+1l)/64;

      bool drripChoice;
      if (IP == INSERT_DIP) {
        // Do this lookup on every access only because we're keeping stats.
        if (dueler.inSample0(set)) {
          if (!h) { dueler.incPsel(); drripChoice = false; }
        } else if (dueler.inSample1(set)) {
          if (!h) { dueler.decPsel(); drripChoice = true; }
        } else {
          drripChoice = dueler.getChoice();
        }
      }

      if (h) {
        ctr[idx] = 0;
      } else {
        if (IP==INSERT_LRU ||
            (IP==INSERT_BIP || (IP==INSERT_DIP && drripChoice))
             && rand() > BRRIP_ALPHA)
        {
          ctr[idx] = RRIP_MAX_STATE - 1;
        } else {
          ctr[idx] = RRIP_MAX_STATE - 2;
        }
      }
    }

    addr_t findVictim(addr_t set) {
      // Look for an invalid line.
      for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i)
        if (!(tagarray[i] & ((1<<L2LINESZ)-1))) return i;

      // Look for counters in max state
      for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i)
        if (ctr[i] == RRIP_MAX_STATE) return i;

      // Increment all of the counters
      for (size_t i = set*WAYS; i < (set+1)*WAYS; ++i) {
        ASSERT(ctr[i] < RRIP_MAX_STATE);
        ++ctr[i];
      }

      // Default to random
      int way;
      do { way = rand(); } while (way > RAND_MAX/WAYS*WAYS);
      way %= WAYS;

      return set*WAYS + way;
    }
    
  private:
    addr_t *tagarray;

    std::vector<unsigned char> ctr;
    SetDueler<WAYS, 5, L2LINESZ, L2SETS, 5> dueler;
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
};

#endif
