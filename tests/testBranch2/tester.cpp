/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), couled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <future>
#include <atomic>

#include <vector>

#include <capstone.h>
#include "cs_disas.h"


#include <qsim.h>
#include <qsim-load.h>

using Qsim::OSDomain;

using std::ostream;

class Tester {
public:
  Tester(OSDomain &osd):
    osd(osd), finished(false), inst(0),
    dis(CS_ARCH_ARM64, CS_MODE_ARM)
  {
    osd.set_app_start_cb(this, &Tester::app_start_cb);
    inst = (uint64_t *)malloc(osd.get_n() * sizeof(uint64_t));
  
    for (int i = 0; i < osd.get_n(); i++) {
      inst[i] = 0;
    }
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_brinst_cb(this, &Tester::brinst_cb);
      osd.set_app_end_cb(this, &Tester::app_end_cb);

      return 0;
    }

    return 0;
  }

  int app_end_cb(int c)
  {
    finished = true;

    return 1;
  }

  void brinst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
               enum inst_type t)
  {
    if (!finished){
      inst[c]++;
      cs_insn *insn = NULL;
      int count = dis.decode((unsigned char *)b, l, insn);
      
      address.push_back(v);
      mnemonic.push_back(insn[0].mnemonic);
      op_str.push_back(insn[0].op_str);

      dis.free_insn(insn, count);
    }
      
  }


  void print_stats(std::ofstream& out)
  {
    for(size_t i = 0; i < address.size(); i++) {
      std::cout << "0x" << std::hex << address[i] << std::dec << " "  << mnemonic[i] << " " << op_str[i] << std::endl;
    }
    uint64_t total_inst = 0;
    for (int i = 0; i < osd.get_n(); i++) {
      total_inst += inst[i];
      std::cout << i << ": " << inst[i] <<  std::endl;
      out       << i << ": " << inst[i] << std::endl;
    }
      std::cout << "Total" << ": " << total_inst << std::endl;
  }

private:
  OSDomain &osd;
  bool finished;
  uint64_t *inst;
  std::vector<uint64_t> address;
  std::vector<std::string> mnemonic;
  std::vector<std::string> op_str;
  cs_disas dis;

};

int main(int argc, char** argv) {
  using std::istringstream;
  using std::ofstream;

  unsigned n_cpus = 1;

  std::string qsim_prefix(getenv("QSIM_PREFIX"));

  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <ncpus> <state_file> <benchmark.tar>\n";
    exit(1);
  }

  istringstream s(argv[1]);
  s >> n_cpus;

  OSDomain *osd_p(NULL);

  // Create new OSDomain from saved state.
  osd_p = new OSDomain(n_cpus, argv[2]);

  OSDomain &osd(*osd_p);
  Tester tw(osd);

  Qsim::load_file(osd, argv[3]);
  std::string bench(argv[3]);
  std::string ofname = bench.substr(0, bench.find(".tar")) + ".out";
  std::ofstream out(ofname);
  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  tw.app_start_cb(0);

  osd.connect_console(std::cout);

  // The main loop: run until 'finished' is true.
  while (!tw.hasFinished()) {
    for (unsigned i = 0; i < n_cpus; i++) {
      osd.run(i, 1000);
      osd.timer_interrupt();
    }
  }

  tw.print_stats(out);

  out.close();

  delete osd_p;

  return 0;
}
