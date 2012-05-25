#include <iostream>

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/time.h>

#include <qsim.h>
#include <qsim-load.h>

extern "C" {
#include "qcache.h"
}

class CallbackAdaptor {
public:
  CallbackAdaptor(struct cache &c,  Qsim::OSDomain &osd):
    c(&c), running(true), icount(0), osd(osd)
  {
    

    icb_handle = osd.set_inst_cb(this, &CallbackAdaptor::inst_cb);
    mcb_handle = osd.set_mem_cb(this, &CallbackAdaptor::mem_cb);
    osd.set_app_end_cb(this, &CallbackAdaptor::app_end_cb);
    
    struct cache *p = &c;
    hit.push_back(0);
    do { hit.push_back(0); } while (p = p->next);
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, 
               const uint8_t *b, enum inst_type t)
  {
    ++icount;
  }

  void mem_cb(int core, uint64_t va, uint64_t pa, uint8_t sz, int wr) {
    ++hit[ac_cache(c, pa, wr)];
  }

  void app_end_cb(int core) {
    running = false;
    osd.unset_inst_cb(icb_handle);
    osd.unset_mem_cb(mcb_handle);
  }

  void print_stats() {
    std::cout << icount << ", ";
    for (unsigned i = 0; i < hit.size(); ++i) {
      std::cout << hit[i] << ", ";
    }
  }

  bool running;

  
private:
  struct cache *c;
  std::vector<unsigned> hit;
  unsigned long long icount;

  Qsim::OSDomain::inst_cb_handle_t icb_handle;
  Qsim::OSDomain::mem_cb_handle_t mcb_handle;
  Qsim::OSDomain &osd;
};

static inline unsigned long long utime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000l*tv.tv_sec + tv.tv_usec;
}

int main(int argc, char** argv) {
  unsigned l1ways = 4, l2ways = 16, l3ways = 32;

  // Build a reasonable 3-level cache hierarchy.
  struct cache c[3];
  init_cache(&c[0], l1ways, 7, 6); //   32k,  4-way set associative L1
  init_cache(&c[1], l2ways, 8, 6); //  256k, 16-way set associative L2
  init_cache(&c[2], l3ways, 13, 6);// 4096k, 32-way set associative L3

  chain_cache(&c[0], &c[1]);
  chain_cache(&c[1], &c[2]);

  Qsim::OSDomain osd(argv[1]);
  std::cout << "State loaded. Loading benchmark.\n";
  osd.connect_console(std::cout);
  Qsim::load_file(osd, argv[2]);
  std::cout << "Benchmark loaded. Running.\n";

  CallbackAdaptor cba(c[0], osd);

  unsigned long long start_usec = utime();
  while (cba.running) {
    osd.run(0, 1000000);
    osd.timer_interrupt();
  }
  unsigned long long end_usec = utime();

  cba.print_stats();
  std::cout << end_usec - start_usec << '\n';

  return 0;
}
