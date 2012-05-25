#include "qcache.h"

void *xmalloc(size_t s) {
  void *ret = calloc(1, s);
  if (ret == NULL) {
    fputs("Malloc failed. Terminating.", stderr);
    exit(1);
  }
  return ret;
}

void init_cache(struct cache *c, int ways, int set_bits, int blocksz_bits) {
  c->ways = ways;
  c->set_bits = set_bits;
  c->blocksz_bits = blocksz_bits;
  c->next = NULL;

  c->tags = xmalloc(ways*(1<<set_bits)*sizeof(*c->tags));
  c->ts_array = xmalloc(ways*(1<<set_bits)*sizeof(*c->ts_array));
  c->ts_max = xmalloc((1<<set_bits)*sizeof(*c->ts_max));
}

void chain_cache(struct cache *upper, struct cache *lower) {
  upper->next = lower;
}
