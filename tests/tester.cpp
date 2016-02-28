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

#include <qsim.h>
#include <qsim-load.h>

using Qsim::OSDomain;

using std::ostream;

class Tester {
public:
  Tester(OSDomain &osd):
    osd(osd), finished(false), inst(0), mem(0), reg(0)
  {
    osd.set_app_start_cb(this, &Tester::app_start_cb);
    inst = (uint64_t *)malloc(osd.get_n() * sizeof(uint64_t));
    mem  = (uint64_t *)malloc(osd.get_n() * sizeof(uint64_t));
    reg  = (uint64_t *)malloc(osd.get_n() * sizeof(uint64_t));

    for (int i = 0; i < osd.get_n(); i++) {
      inst[i] = 0;
      mem[i]  = 0;
      reg[i]  = 0;
    }
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &Tester::inst_cb);
      osd.set_mem_cb(this, &Tester::mem_cb);
      osd.set_reg_cb(this, &Tester::reg_cb);
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

  void reg_cb(int c, int r, uint8_t s, int type)
  {
    if (!finished)
      reg[c]++;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
               enum inst_type t)
  {
    if (!finished)
      inst[c]++;

    //std::cout << "inst: " << inst << "\r";
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w)
  {
    if (!finished)
      mem[c]++;
  }

  void print_stats(std::ofstream& out)
  {
    for (int i = 0; i < osd.get_n(); i++) {
      std::cout << i << ": " << inst[i] << ", " << mem[i] << ", " << reg[i] << std::endl;
      out       << i << ": " << inst[i] << ", " << mem[i] << ", " << reg[i] << std::endl;
    }
  }

private:
  OSDomain &osd;
  bool finished;
  uint64_t *inst, *mem, *reg;
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
    for (unsigned i = 0; i < n_cpus; i++)
      osd.run(i, 1000);
    osd.timer_interrupt();
  }

  tw.print_stats(out);

  out.close();

  delete osd_p;

  return 0;
}
