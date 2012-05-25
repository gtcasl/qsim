#ifndef __QCACHE_H
#define __QCACHE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

typedef uint64_t addr_t;

// blocksz_bits is the log2 of the block size and must be at least large enough
// to make room for attribute bits in the tag array.
struct cache {
  int ways, set_bits, blocksz_bits;
  addr_t *tags;
  unsigned *ts_array, *ts_max;
  struct cache *next;
};

enum { CACHE_ATTR_VALID = 0x01,
       CACHE_ATTR_DIRTY = 0x02 };

void init_cache(struct cache *c, int ways, int set_bits, int blocksz_bits);
void chain_cache(struct cache *upper, struct cache *lower);

// Update the LRU information for a successful access.
static inline void upd_lru(struct cache *c, addr_t set, int idx) {
  // No need to update our timestamp if this is already the MRU way.
  if (c->ts_max[set] == c->ts_array[idx]) return;

  // In the event of a timestamp overflow, react accordingly.
  if (c->ts_max[set] == UINT_MAX) {
    fputs("Timestamp overflow. Terminating.", stderr);
    exit(2);
  }

  // Increment the max timestamp and timestamp the accessed line.
  ++c->ts_max[set];
  c->ts_array[idx] = c->ts_max[set];
}

// Access the cache.
static inline int ac_cache(struct cache *c, addr_t a, int wr)
{
  addr_t set = (a>>c->blocksz_bits) & ((1<<c->set_bits)-1);
  int idx = set * c->ways;

  int i;
  for (i = idx; i < idx + c->ways; i++) {
    addr_t tag = c->tags[i];
    // Read left side as "the MSBs of the tag array entry and address match"
    if (!((tag ^ a) >> c->blocksz_bits) && (tag & CACHE_ATTR_VALID)) {
      if (wr) c->tags[i] |= CACHE_ATTR_DIRTY;
      upd_lru(c, set, i);
      //puts("Hit.");
      return 0;
    }
  };

  // There has been a miss. Find the line with the minimum timestamp or without
  // its valid bit set.
  if (c->tags[idx] & CACHE_ATTR_VALID) {
    unsigned min_ts = c->ts_array[idx];
    for (i = idx+1; i < idx + c->ways; i++) {
      if (!(c->tags[i] & CACHE_ATTR_VALID)) {
        idx = i;
        break;
      } else if (c->ts_array[i] < min_ts) {
        idx = i; min_ts = c->ts_array[i];
      }
    }
  }
#ifdef DBG
  printf("Miss. Using way %u of %u, valid = %u, set=%u, cache=%p\n",
         idx%c->ways, c->ways, c->tags[idx] & CACHE_ATTR_VALID, idx/c->ways, c);
#endif
  if (c->next == NULL) {
    if (c->tags[idx] & CACHE_ATTR_VALID) {
      unsigned long long ev_addr =
        (c->tags[idx] & ~(addr_t)((1<<c->blocksz_bits)-1));
      fprintf(stderr, "%llu, W\n", ev_addr);
    }
    fprintf(stderr, "%llu, R\n", a & ~(addr_t)((1<<c->blocksz_bits)-1));
  }

  if (c->tags[idx] & CACHE_ATTR_VALID && c->next != NULL) {
    // Do a proper eviction and actually write the line back.
    ac_cache(c->next, c->tags[idx] & ~(addr_t)((1<<c->blocksz_bits)-1), 1);
  }

  c->tags[idx] = (a & ~(addr_t)((1<<c->blocksz_bits)-1)) | CACHE_ATTR_VALID;
  if (wr) c->tags[idx] |= CACHE_ATTR_DIRTY;

  // And then update the LRU info.
  upd_lru(c, set, idx);

  // Do it all over again for the next level down.
  if (!c->next) return 1;
  return 1 + ac_cache(c->next, a, wr);
} 

#endif
