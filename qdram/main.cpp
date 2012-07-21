#include "qdram-config.h"
#include "qdram.h"
#include "qdram-sched.h"

#include <iostream>
#include <iomanip>
#include <fstream>

typedef Qcache::MemController<Qcache::DramTiming1067,
                              Qcache::Dim4GB2Rank,
                              Qcache::AddrMappingB> mc_t;

int main(int argc, char** argv) {
  mc_t *mc(new mc_t());

  if (argc < 2) return 1;
  std::ifstream infile(argv[1]);

  bool readAddr(true), write;
  Qcache::addr_t addr;
  while (!!infile || !mc->empty()) {
    mc->tickBegin();

    if (!!infile) {
      if (readAddr) {
        char c;
        infile >> addr >> c;
        write = (c == 'W');
      }

      readAddr = mc->access(addr, write);
    }

    mc->tickEnd();
    mc->printStats();
  }

  mc->printStats();

  delete(mc);

  return 0;
}
