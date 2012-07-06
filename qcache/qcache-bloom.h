// A fairly ordinary bloom filter implementation. Hashes are random binary
// matrices. Can count false positives and produce a report in destructor if
// desired (FPCOUNT parameter).
#ifndef __BLOOM_H
#define __BLOOM_H

#include <set>
#include <cstdlib>
#include <stdint.h>

namespace Qcache {
  template <unsigned L2_BITS, unsigned HASHES, bool FPCOUNT=false>
    class BloomFilter
  {
  public:
    BloomFilter(unsigned int rseed = 0): aCount(0), hitCount(0), fpCount(0) {
      // Clear all bits in the filter.
      clear();

      // Generate some hash matrices.
      for (unsigned i = 0; i < HASHES; ++i) {
        for (unsigned j = 0; j < L2_BITS; ++j) {
          uint64_t hash = 0;
          for (unsigned k = 0; k < 64; ++k) {
            hash <<= 1;
            hash |= (rand_r(&rseed) < RAND_MAX/2);
          }
	  hashes[i][j] = hash;
        }
      }
    }

    ~BloomFilter() {
      if (FPCOUNT) {
	std::cout << "2^" << L2_BITS << " bits, " << HASHES << " hash func., "
	          << aCount << " accesses, " << fpset.size() << " unique, "
                  << hitCount << " hits, " << fpCount << " false pos.\n";
      }
    }

    void add(uint64_t val) {
      for (unsigned i = 0; i < HASHES; ++i) {
        unsigned b = bitIdx(val, i);
        bits[b/64] |= 1ll<<(b%64);
      }

      if (FPCOUNT) { fpset.insert(val); }
    }

    bool check(uint64_t val) {
      bool p(true);
      for (unsigned i = 0; i < HASHES; ++i) {
        unsigned b = bitIdx(val, i);
        if (!(bits[b/64] & (1ll<<(b%64)))) p = false;
      }

      if (FPCOUNT) {
        ++aCount;
        if (p) ++hitCount;
        if (p && fpset.find(val) == fpset.end()) ++fpCount;
      }

      return p;
    }

    void clear() {
      const unsigned N = L2_BITS<6 ? 1 : (1ll<<(L2_BITS-6));
      for (unsigned i = 0; i < N; ++i) bits[i] = 0;
  
      if (FPCOUNT) fpset.clear();
    }

    void print() {
      for (unsigned i = 0; i < (1ll<<L2_BITS); ++i) {
	std::cout << ((bits[i/64] & (1ll<<(i%64)))?'1':'0');
      }
      std::cout << '\n';
    }

 private:
   uint64_t bits[L2_BITS<6 ? 1 : (1ll<<((L2_BITS-6)))], hashes[HASHES][L2_BITS],
            aCount, hitCount, fpCount;
   std::set<uint64_t> fpset;

   unsigned bitIdx(uint64_t val, unsigned hash) {
     unsigned bitIdx = 0;
     for (unsigned i = 0; i < L2_BITS; ++i) {
       bitIdx <<= 1;
       bitIdx |= __builtin_parityll(hashes[hash][i] & val);
     }
     return bitIdx;
   }
  };
};

#endif
