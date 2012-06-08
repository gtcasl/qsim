
#include <iostream>
#include <fstream>
#include <vector>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/time.h>

#include <qsim.h>
#include <qsim-load.h>

#include "qcache.h"

// <Coherence Protorol, Ways, log2(sets), log2(bytes/line)>
// Last parameter of L3 cache type says that it's shared.
typedef Qcache::CacheGrp<Qcache::CPNull,     4,  7, 6      > l1i_t;
typedef Qcache::CacheGrp<Qcache::CPDirMoesi, 8,  6, 6      > l1d_t;
typedef Qcache::CacheGrp<Qcache::CPNull,     8,  8, 6      > l2_t;
typedef Qcache::Cache   <Qcache::CPNull,    24, 14, 6, true> l3_t;

class CallbackAdaptor {
public:
  CallbackAdaptor(Qsim::OSDomain &osd, l1i_t &l1i, l1d_t &l1d):
    running(true), icount(0), l1i(l1i), l1d(l1d), osd(osd)
  {
    
    icb_handle = osd.set_inst_cb(this, &CallbackAdaptor::inst_cb);
    mcb_handle = osd.set_mem_cb(this, &CallbackAdaptor::mem_cb);
    osd.set_app_end_cb(this, &CallbackAdaptor::app_end_cb);
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, 
               const uint8_t *b, enum inst_type t)
  {
    ++icount;
    l1i.getCache(c).access(p, false);
  }

  void mem_cb(int c, uint64_t va, uint64_t pa, uint8_t sz, int wr) {
    l1d.getCache(c).access(pa, wr);
  }

  void app_end_cb(int core) {
    running = false;

    osd.unset_inst_cb(icb_handle);
    osd.unset_mem_cb(mcb_handle);
  }

  bool running;

  
private:
  unsigned long long icount;

  l1i_t &l1i;
  l1d_t &l1d;

  Qsim::OSDomain::inst_cb_handle_t icb_handle;
  Qsim::OSDomain::mem_cb_handle_t mcb_handle;
  Qsim::OSDomain &osd;
};

static inline unsigned long long utime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000l*tv.tv_sec + tv.tv_usec;
}


pthread_barrier_t b0, b1;
Qsim::OSDomain *osd_p;
CallbackAdaptor *cba_p;

struct thread_arg_t {
  int cpu;
  pthread_t thread;
};

void *thread_main(void *arg_vp) {
  bool running = true;
  thread_arg_t *arg((thread_arg_t*)arg_vp);

  pthread_barrier_wait(&b1);

  while (cba_p->running) {
    osd_p->run(arg->cpu, 1000000);

    pthread_barrier_wait(&b0);
    if (arg->cpu == 0) {
      if (!cba_p->running) {
        running = false;
      } else {
        osd_p->timer_interrupt();
      }
    }
    pthread_barrier_wait(&b1);  
  }

  return 0;
}

int main(int argc, char** argv) {
  Qsim::OSDomain osd(argv[1]);
  std::cout << "State loaded. Loading benchmark.\n";
  osd.connect_console(std::cout);
  Qsim::load_file(osd, argv[2]);
  std::cout << "Benchmark loaded. Running.\n";

  std::ostream *traceOut;
  if (argc >= 4) {
    traceOut = new std::ofstream(argv[3]);
  } else {
    traceOut = &std::cout;
  }

  // Build a Westmere-like 3-level cache hierarchy. Typedefs for these (which
  // determine the cache parameters) are at the top of the file.
  Qcache::Tracer tracer(*traceOut);
  l3_t l3(tracer);
  l2_t l2(osd.get_n(), l3);
  l1d_t l1_d(osd.get_n(), l2);
  l1i_t l1_i(osd.get_n(), l2);

  pthread_barrier_init(&b0, NULL, osd.get_n());
  pthread_barrier_init(&b1, NULL, osd.get_n());

  CallbackAdaptor cba(osd, l1_i, l1_d);

  osd_p = &osd;
  cba_p = &cba;

  std::vector<thread_arg_t> targs(osd.get_n());  

  unsigned long long start_usec = utime();
  for (unsigned i = 0; i < osd.get_n(); ++i) {
    targs[i].cpu = i;
    pthread_create(&targs[i].thread, NULL, thread_main, (void*)&targs[i]);
  }

  for (unsigned i = 0; i < osd.get_n(); ++i)
    pthread_join(targs[i].thread, NULL);
  unsigned long long end_usec = utime();

  std::cout << "Total time: " << std::dec << end_usec - start_usec << "us\n";

  if (argc >= 4) {
    delete traceOut;
  }

  return 0;
}
