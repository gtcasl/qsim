#include "data.h"
#include "cache.h"
#include "cpu.h"
#include "des.h"

#include <qsim.h>

#include <vector>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <cstdlib>

template <typename T> bool sstreamRead(T& dest, const char *src) {
  std::istringstream is(src);

  is >> dest;

  return !!is;
}

int Main(int argc, char **argv) {
  using std::cout; using std::vector;
  using SimpleSim::SuperscalarCpu; using SimpleSim::SimpleCpu; using SimpleSim::Cache;
  using SimpleSim::DramController;

  unsigned nCpus, rSeed(0);
  uint64_t nCycles;

  // Read arguments; print usage if we don't have the right number.
  if (argc < 3 || argc > 4 ||
                    !sstreamRead(nCpus, argv[1]) || nCpus == 0 ||
                    !sstreamRead(nCycles, argv[2]) ||
      (argc >= 4 && !sstreamRead(rSeed, argv[3])))
  {
    cout << "Usage:\n  " << argv[0] << " <# CPUs> <# Cycles>\n";
    return 1;
  }

  srand(rSeed);

  // Instantiate QSIM with appropriate state file or whatever it needs.
  Qsim::OSDomain osd(nCpus, "bzImage");
  osd.connect_console(cout);

  // Instantiate CPUs.
#ifdef SIMPLE_CPU
  vector<SimpleCpu*> cpus;
#else
  vector<SuperscalarCpu*> cpus;
#endif

  for (unsigned i = 0; i < nCpus; ++i) {
#ifdef SIMPLE_CPU
    cpus.push_back(new SimpleCpu(i, osd));
#else
    cpus.push_back(new SuperscalarCpu(i, osd, 64, 4, 4));
    //             Op Type,        Res. Stations, #Func. Units, Lat., Interval
    cpus[i]->addFu(QSIM_INST_NULL,            16,           16,    3,        1);
    cpus[i]->addFu(QSIM_INST_INTBASIC,         6,            4,    1,        1);
    cpus[i]->addFu(QSIM_INST_INTMUL,           6,            1,    4,        1);
    cpus[i]->addFu(QSIM_INST_INTDIV,           6,            1,    8,        4);
    cpus[i]->addFu(QSIM_INST_STACK,            6,            2,    1,        1);
    cpus[i]->addFu(QSIM_INST_FPBASIC,          6,            2,    8,        1);
    cpus[i]->addFu(QSIM_INST_FPMUL,            3,            1,   16,        4);
    cpus[i]->addFu(QSIM_INST_FPDIV,            3,            1,   32,       16);
#endif
  }

  // Instantiate L1 caches. (nCpus*2 of them if we have separate I and D cache)
  vector <Cache*>l1i, l1d;
  for (unsigned i = 0; i < nCpus; ++i) {
    l1i.push_back(new Cache(1, i, 5, 8, 4, "i")); // 1-way, 32k L1i
    l1d.push_back(new Cache(1, i, 5, 8, 4, "d")); // 1-way, 32k L1d
    cpus[i]->setCache(l1i[i], l1d[i]);
  }

  // Connect L1 caches together.
  for (unsigned i = 0; i < nCpus; ++i) {
    l1i[i]->addPeer(l1d[i]);
    l1d[i]->addPeer(l1i[i]);
    for (unsigned j = 0; j < nCpus; ++j) {
      if (i == j) continue;
      l1i[i]->addPeer(l1i[j]);
      l1i[i]->addPeer(l1d[j]);
      l1d[i]->addPeer(l1i[j]);
      l1d[i]->addPeer(l1d[j]);
    }
  }

  // Instantiate L2 cache.
  Cache l2(2, 0, 5, 13, 4); // 4-way 1024k L2
  for (unsigned i = 0; i < nCpus; ++i) {
    l1i[i]->setLowerLevel(&l2);
    l1d[i]->setLowerLevel(&l2);
  }

  // Instantiate Memory controller (200 cycle round trip).
  DramController mc(200);
  l2.setLowerLevel(&mc);

  // Run everything for a while.
  if (nCycles == 0) {
    while(Slide::_advance(Slide::_now + 1000)) {
      SimpleSim::Counter::printAll(cout);
      SimpleSim::Counter::resetAll();
    }
  } else {
    while (nCycles) {
      unsigned n(nCycles>=1000?1000:nCycles);
      Slide::_advance(Slide::_now + n);
      SimpleSim::Counter::printAll(cout);
      SimpleSim::Counter::resetAll();
      nCycles -= n;
    }
  }

  SimpleSim::Counter::printAll(cout);
  SimpleSim::Counter::resetAll();

  cout << "Finished at cycle " << Slide::_now << ".\n";

  return 0;
}

// The real main function, calls our main function and catches any exceptions.
// All of the SimpleSim exceptions are C strings, so it just prints the error
// and exits.
int main(int argc, char** argv) {
  using std::cout;
  try {
    return Main(argc, argv); 
  } catch (const char *s) {
    cout << "Caught exception: \"" << s << "\".\n";
    return 1;
  }
}
