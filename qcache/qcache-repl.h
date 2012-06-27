#ifndef __QCACHE_REPL_H
#define __QCACHE_REPL_H

#include "qcache.h"

namespace Qcache {
  enum InsertionPolicy {
    INSERT_LRU, INSERT_MRU, INSERT_BIP
  };

  template
    <int WAYS, int L2SETS, int L2LINESZ, InsertionPolicy IP>
  class ReplLRUBase {
   public:
     ReplLRUBase(std::vector<addr_t> &ta) :
       tagarray(&ta[0]), tsarray(size_t(WAYS)<<L2SETS),
       tsmax(size_t(1)<<L2SETS) {}

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

      if (IP != INSERT_LRU) {
        if (IP == INSERT_BIP && rand() <= BIP_ALPHA) return;
        tsarray[idx] = 0;        
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
    typedef unsigned timestamp_t;
      #define TIMESTAMP_MAX UINT_MAX

    addr_t *tagarray;
    std::vector<timestamp_t> tsarray, tsmax;
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
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_LRU> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_MIP {
  public:
    ReplLRU_MIP(std::vector<addr_t> &ta):  r(ta) { }

    QCACHE_REPL_PASSTHROUGH_FUNCS

  private:
    ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_MRU> r;
  };

  template <int WAYS, int L2SETS, int L2LINESZ> class ReplLRU_BIP {
   public:
     ReplLRU_BIP(std::vector<addr_t> &ta):  r(ta) { }

     QCACHE_REPL_PASSTHROUGH_FUNCS

   private:
     ReplLRUBase<WAYS, L2SETS, L2LINESZ, INSERT_BIP> r;
  };

};

#endif
