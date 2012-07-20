#include "qdram-config.h"

#include <iostream>
#include <iomanip>
#include <fstream>

int main(int argc, char** argv) {
  if (argc < 2) return 1;
  std::ifstream infile(argv[1]);

  Qcache::AddrMappingA<Qcache::Dim4GB2Rank> m;

  while (!!infile) {
    if (!!infile) {
      char c; Qcache::addr_t addr;
      infile >> addr >> c;
      std::cout << m.getChannel(addr) << ',' << m.getRank(addr) << ','
                << m.getBank(addr) << ',' << m.getRow(addr) << ','
                << m.getCol(addr) << ' ' << c << '\n';
    }
  }

  return 0;
}
