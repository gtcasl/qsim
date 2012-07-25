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

#include <qcache.h>
#include <qcache-moesi.h>
#include <qcache-repl.h>

#include <qcpu.h>

#include <qdram-sched.h>

#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

//#define ICOUNT
#define CPULOCK
//#define PROFILE

#ifdef PROFILE
#include <qsim-prof.h>
#endif

#ifdef ICOUNT
  #define ICOUNT_MAX_CORES 256
  uint64_t icount[ICOUNT_MAX_CORES];
  uint64_t idlecount[ICOUNT_MAX_CORES];
#endif

using Qcache::ReplLRU;     using Qcache::CacheGrp;   using Qcache::Cache;
using Qcache::CPNull;      using Qcache::CPDirMoesi; using Qcache::ReplRand;
using Qcache::ReplLRU_BIP; using Qcache::ReplDRRIP;  using Qcache::ReplLRU_DIP;
using Qcache::ReplLRU_LIP; using Qcache::ReplSRRIP;  using Qcache::ReplBRRIP;
using Qcache::ReplLRU_EAF; using Qcache::ReplERRIP;

// <Coherence Protocol, Ways, log2(sets), log2(bytes/line), Replacement Policy>
// Last parameter of L3 cache type says that it's shared.
typedef Qcache::CacheGrp< 0, CPNull,     4,  7, 6, ReplLRU         > l1i_t;
typedef Qcache::CacheGrp< 0, CPDirMoesi, 8,  6, 6, ReplRand        > l1d_t;
typedef Qcache::CacheGrp<10, CPNull,     8,  8, 6, ReplRand        > l2_t;
typedef Qcache::Cache   <20, CPNull,    16, 9, 6, Qcache::ReplRand,  true> l3_t;

typedef Qcache::MemController<Qcache::DramTiming1067,
                              Qcache::Dim4GB2Rank,
                              Qcache::AddrMappingB, 3> mc_t;

//typedef Qcache::CPUTimer<Qcache::InstLatencyForward, mc_t> CPUTimer_t;
typedef Qcache::OOOCpuTimer<mc_t, 6, 4, 64> CPUTimer_t;

std::vector <bool> Qcache::dramUseFlag;
std::vector <bool*> Qcache::dramFinishedFlag;
int Qcache::dramAdditionalLatency;

// Tiny 512k LLC to use (without L2) when validating replacement policies
//typedef Qcache::Cache   <CPNull,   8, 10, 6, ReplBRRIP, true> l3_t;

// This is a sad little hack that ensures our N threads are packed into the N
// lowest-ID'd CPUs. On our test machine, this keeps the threads on as few
// sockets as possible, without scheduling any pair of threads to the same
// hyperthreaded core (as long as there are enough real cores available).
#ifdef CPULOCK
void setCpuAff(int threads) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (int i = 0; i < threads; ++i) CPU_SET(i, &mask);
  sched_setaffinity(getpid(), threads, &mask);
}
#endif

class CallbackAdaptor {
public:
  CallbackAdaptor(Qsim::OSDomain &osd, l1i_t &l1i, l1d_t &l1d, mc_t &mc):
    cpu(), running(true), l1i(l1i), l1d(l1d), mc(mc), osd(osd)
  {
    icb_handle = osd.set_inst_cb(this, &CallbackAdaptor::inst_cb);
    mcb_handle = osd.set_mem_cb(this, &CallbackAdaptor::mem_cb);
    osd.set_reg_cb(this, &CallbackAdaptor::reg_cb);
    osd.set_app_end_cb(this, &CallbackAdaptor::app_end_cb);

    for (unsigned i = 0; i < osd.get_n(); ++i) {
      cpu.push_back(CPUTimer_t(i, l1d.getCache(i), l1i.getCache(i), mc));
    }

    #ifdef PROFILE
    Qsim::start_prof(osd, "QSIM_PROF", 10000000, 10);
    #endif
  }

  ~CallbackAdaptor() {
    #ifdef ICOUNT
    for (unsigned i = 0; i < osd.get_n(); ++i) {
      std::cout << "Instructions/idle for CPU " << i << ", " << icount[i]
                << ", " << idlecount[i] << '\n';
    }
    #endif
    //osd.unset_inst_cb(icb_handle);
    //osd.unset_mem_cb(mcb_handle);

    #ifdef PROFILE
    Qsim::end_prof(osd);
    #endif
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, 
               const uint8_t *b, enum inst_type t)
  {
    if (!running/* || osd.get_prot(c) == Qsim::OSDomain::PROT_KERN*/) return;
    #ifdef ICOUNT
    ++icount[c];
    if (osd.idle(c)) ++idlecount[c];
    if (icount[c] == 1000) {
      running = false;
      return;
    }
    #endif
    cpu[c].instCallback(p, t);
    //l1i.getCache(c).access(p, p, c, 0);
  }

  void reg_cb(int c, int r, uint8_t size, int wr) {
    if (!running/* || osd.get_prot(c) == Qsim::OSDomain::PROT_KERN*/) return;
    cpu[c].regCallback(size==0?QSIM_RFLAGS:regs(r), wr);
  }

  void mem_cb(int c, uint64_t va, uint64_t pa, uint8_t sz, int wr) {
    if (!running/* || osd.get_prot(c) == Qsim::OSDomain::PROT_KERN*/) return;
    //l1d.getCache(c).access(pa, osd.get_reg(c, QSIM_RIP), c, wr);
    cpu[c].memCallback(pa, osd.get_reg(c, QSIM_RIP), wr);
  }

  int app_end_cb(int core) {
    running = false;
    return 1;
  }

  bool running;
  
  l1i_t &l1i;
  l1d_t &l1d;
  mc_t &mc;

  Qsim::OSDomain::inst_cb_handle_t icb_handle;
  Qsim::OSDomain::mem_cb_handle_t mcb_handle;
  Qsim::OSDomain &osd;

  std::vector<CPUTimer_t> cpu;
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
  int cpuStart;
  int cpuEnd;
  pthread_t thread;
};

void *thread_main(void *arg_vp) {
  bool running = true;
  thread_arg_t *arg((thread_arg_t*)arg_vp);

  pthread_barrier_wait(&b0);
  while (cba_p->running) {
    //pthread_barrier_wait(&b0);
    for (unsigned i = 0; i < 500; ++i) {
      for (unsigned c = arg->cpuStart; c < arg->cpuEnd; ++c) {
        cba_p->cpu[c].updateCycle();
        osd_p->run(c, 1000);
       }
      if (!cba_p->running) break;
    }

    if (arg->cpuStart == 0) {
      if (!cba_p->running) {
        running = false;
      } else {
        osd_p->timer_interrupt();
      }
    }

    //pthread_barrier_wait(&b1);  
  }

  return 0;
}

int main(int argc, char** argv) {
  int threads;

  if (argc < 3) {
    std::cout << "Usage:\n  " << argv[0] << " <state file> "
              << "<benchmark tar file> <# host threads> [trace file]\n";
    exit(1);
  }

  Qsim::OSDomain osd(argv[1]);
  std::cout << "State loaded. Loading benchmark.\n";
  osd.connect_console(std::cout);
  Qsim::load_file(osd, argv[2]);
  std::cout << "Benchmark loaded. Running.\n";

  if (argc >= 4) {
    threads = atol(argv[3]);
  } else {
    threads = osd.get_n();
  }

  Qcache::dramFinishedFlag.resize(osd.get_n());
  Qcache::dramUseFlag.resize(osd.get_n());

  std::ostream *traceOut;
  if (argc >= 5) {
    traceOut = new std::ofstream(argv[4]);
  } else {
    traceOut = &std::cout;
  }

  // Build a Westmere-like 3-level cache hierarchy. Typedefs for these (which
  // determine the cache parameters) are at the top of the file.
  //Qcache::Tracer tracer(*traceOut);
  mc_t mc(osd.get_n());
  l3_t l3(mc, "L3");
  l2_t l2(osd.get_n(), l3, "L2");
  l1i_t l1_i(osd.get_n(), l2, "L1i");
  l1d_t l1_d(osd.get_n(), l2, "L1d");

  pthread_barrier_init(&b0, NULL, threads);
  pthread_barrier_init(&b1, NULL, threads);

  CallbackAdaptor *cba = new CallbackAdaptor(osd, l1_i, l1_d, mc);

  osd_p = &osd;
  cba_p = cba;

  std::vector<thread_arg_t> targs(threads);  

  if (osd.get_n() % threads) {
    std::cerr << "Error: number of host threads must divide evenly into"
                 " number of guest threads.\n";
    exit(1);
  }

#ifdef CPULOCK
  setCpuAff(threads);
#endif

  unsigned long long start_usec = utime();
  for (unsigned i = 0; i < threads; ++i) {
    targs[i].cpuStart = i * (osd.get_n() / threads);
    targs[i].cpuEnd = (i + 1) * (osd.get_n() / threads);
    pthread_create(&targs[i].thread, NULL, thread_main, (void*)&targs[i]);
  }

  for (unsigned i = 0; i < threads; ++i)
    pthread_join(targs[i].thread, NULL);
  unsigned long long end_usec = utime();

  delete cba;

  std::cout << "Total time: " << std::dec << end_usec - start_usec << "us\n";

  Qcache::printResults = true;

  if (argc >= 5) {
    delete traceOut;
  }

  return 0;
}
