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
#include <qtickable.h>

#include <qcpu.h>

//#include <qdram-sched.h>

#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

#define CPULOCK
//#define PROFILE

#ifdef PROFILE
#include <qsim-prof.h>
#endif

using Qcache::ReplLRU;     using Qcache::CacheGrp;   using Qcache::Cache;
using Qcache::CPNull;      using Qcache::CPDirMoesi; using Qcache::ReplRand;
using Qcache::ReplLRU_BIP; using Qcache::ReplDRRIP;  using Qcache::ReplLRU_DIP;
using Qcache::ReplLRU_LIP; using Qcache::ReplSRRIP;  using Qcache::ReplBRRIP;
using Qcache::ReplLRU_EAF; using Qcache::ReplERRIP;  using Qcache::ReplSHIP;

using Qcache::DramTiming1067;
using Qcache::Dim4GB1Rank;
using Qcache::Dim4GB2Rank;
using Qcache::AddrMappingA;
using Qcache::MemController;

// <Coherence Protocol, Ways, log2(sets), log2(bytes/line), Replacement Policy>
// Last parameter of L3 cache type says that it's shared.
typedef Qcache::CacheGrp< 0, CPNull,     4,  7, 6, ReplLRU         > l1i_t;
typedef Qcache::CacheGrp< 0, CPDirMoesi, 8,  6, 6, ReplLRU        > l1d_t;
typedef Qcache::CacheGrp<10, CPNull,     8,  8, 6, ReplLRU        > l2_t;
typedef Qcache::Cache   <20, CPNull,    16, 9, 6, ReplERRIP,  true> l3_t;

// For now the L1 through LLC latency is a template parameter to mc_t.
//typedef MemController<DramTiming1067, Dim4GB2Rank, AddrMappingA,30,3> mc_t;
typedef Qcache::FuncDram<200, 100, 3, Dim4GB2Rank, AddrMappingA> mc_t;

typedef Qcache::CPUTimer<Qcache::InstLatencyForward, 2> CPUTimer_t;
//typedef Qcache::OOOCpuTimer<6, 4, 64> CPUTimer_t;

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
  CallbackAdaptor(
    Qsim::OSDomain &osd, l1i_t &l1i, l1d_t &l1d, Qcache::Tickable *mc=NULL
  ):
    cpu(), running(true), l1i(l1i), l1d(l1d), osd(osd)
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
    //osd.unset_inst_cb(icb_handle);
    //osd.unset_mem_cb(mcb_handle);

    #ifdef PROFILE
    Qsim::end_prof(osd);
    #endif
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, 
               const uint8_t *b, enum inst_type t)
  {
    if (!running || osd.get_prot(c) == Qsim::OSDomain::PROT_KERN)
      cpu[c].idleInst();

    cpu[c].instCallback(p, t);
    //l1i.getCache(c).access(p, p, c, 0);
  }

  void reg_cb(int c, int r, uint8_t size, int wr) {
    if (!running || osd.get_prot(c) == Qsim::OSDomain::PROT_KERN) return;
    cpu[c].regCallback(size==0?QSIM_RFLAGS:regs(r), wr);
  }

  void mem_cb(int c, uint64_t va, uint64_t pa, uint8_t sz, int wr) {
    if (!running || osd.get_prot(c) == Qsim::OSDomain::PROT_KERN) return;
    //l1d.getCache(c).access(pa, osd.get_reg(c, QSIM_RIP), c, wr);
    cpu[c].memCallback(pa, osd.get_reg(c, QSIM_RIP), wr);
  }

  int app_end_cb(int core) {
    running = false;
    return 1;
  }

  volatile bool running;
  
  l1i_t &l1i;
  l1d_t &l1d;

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
  Qcache::cycle_t nextBarrier;
  pthread_t thread;
};

// Number of cycles between barriers.
const Qcache::cycle_t BARRIER_INTERVAL = 1000000;

void *thread_main(void *arg_vp) {
  bool runningLocal(true);

  thread_arg_t *arg((thread_arg_t*)arg_vp);

  arg->nextBarrier = BARRIER_INTERVAL;

  pthread_barrier_wait(&b0);
  while(runningLocal) {
    bool doBarrier(true);
    for (unsigned i = 0; i < 2000; ++i) {
      for (unsigned c = arg->cpuStart; c < arg->cpuEnd; ++c) {
        if (cba_p->cpu[c].getCycle() >= arg->nextBarrier) continue;
	bool runFail(osd_p->run(c, 100) == 0);
        if (!runFail && cba_p->cpu[c].getCycle() < arg->nextBarrier)
          doBarrier = false;

        // Even if we run out of cycles, the CPU model must be ticked.
        if (runFail)
          while (cba_p->cpu[c].getCycle() < arg->nextBarrier)
            cba_p->cpu[c].idleInst();
      }
      if (doBarrier) {
          arg->nextBarrier += BARRIER_INTERVAL;
          pthread_barrier_wait(&b1);
          // Every thread reads "running" only in this critical section.
          runningLocal = cba_p->running;
          if (!runningLocal) break;
          pthread_barrier_wait(&b0);
      }
    }

    if (arg->cpuStart == 0 && runningLocal) osd_p->timer_interrupt();
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

  std::vector<thread_arg_t> targs(threads);

  std::ostream *traceOut;
  if (argc >= 5) {
    traceOut = new std::ofstream(argv[4]);
  } else {
    traceOut = &std::cout;
  }

  // Build a Westmere-like 3-level cache hierarchy. Typedefs for these (which
  // determine the cache parameters) are at the top of the file.
  //Qcache::Tracer tracer(*traceOut);
  //mc_t mc(osd.get_n()); // For qdram
  mc_t mc; // For FuncDram
  l3_t l3(mc, "L3");
  l2_t l2(osd.get_n(), l3, "L2");
  l1i_t l1_i(osd.get_n(), l2, "L1i");
  l1d_t l1_d(osd.get_n(), l2, "L1d");

  pthread_barrier_init(&b0, NULL, threads);
  pthread_barrier_init(&b1, NULL, threads);

  //CallbackAdaptor *cba = new CallbackAdaptor(osd, l1_i, l1_d, &mc); // qdram
  CallbackAdaptor *cba = new CallbackAdaptor(osd, l1_i, l1_d); // FuncDram

  osd_p = &osd;
  cba_p = cba;

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

  std::cout << "Total time: " << std::dec << end_usec - start_usec << "us\n";

  Qcache::printResults = true;

  delete cba;

  if (argc >= 5) {
    delete traceOut;
  }

  return 0;
}
