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

#include "distorm.h"

#include <qsim.h>
#include <qsim-load.h>

using Qsim::OSDomain;

using std::ostream;

class TraceWriter {
public:
  TraceWriter(OSDomain &osd) : 
    osd(osd), finished(false) 
  { 
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb); 
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_magic_cb(this, &TraceWriter::magic_cb);
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
    }

    return 0;
  }

  int magic_cb(int c, uint64_t rax)
  {
    if (rax == 0xc5b1fffd) {
      uint64_t vaddr = osd.get_reg(c, QSIM_X86_RBX);
      uint32_t val = 0xfafa;
      osd.mem_wr_virt(c, val, vaddr);
      osd.set_reg(c, QSIM_X86_RCX, sizeof(val));
    }

    return 0;
  }

  int app_end_cb(int c)   { finished = true; return 0; }

private:
  OSDomain &osd;
  bool finished;

  static const char * itype_str[];
};

const char *TraceWriter::itype_str[] = {
  "QSIM_INST_NULL",
  "QSIM_INST_INTBASIC",
  "QSIM_INST_INTMUL",
  "QSIM_INST_INTDIV",
  "QSIM_INST_STACK",
  "QSIM_INST_BR",
  "QSIM_INST_CALL",
  "QSIM_INST_RET",
  "QSIM_INST_TRAP",
  "QSIM_INST_FPBASIC",
  "QSIM_INST_FPMUL",
  "QSIM_INST_FPDIV"
};

int main(int argc, char** argv) {
  using std::istringstream;
  using std::ofstream;

  ofstream *outfile(NULL);

  unsigned n_cpus = 1;

  std::string qsim_prefix(getenv("QSIM_PREFIX"));

  // Read number of CPUs as a parameter. 
  if (argc >= 2) {
    istringstream s(argv[1]);
    s >> n_cpus;
  }

  OSDomain *osd_p(NULL);

  if (argc >= 3) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(n_cpus, argv[2]);
  } else {
    osd_p = new OSDomain(n_cpus, qsim_prefix + "/../x86_64_images/vmlinuz", "x86", QSIM_INTERACTIVE);
  }
  OSDomain &osd(*osd_p);

  std::string benchmark;
  if (argc >= 4) {
    istringstream bench_file(argv[3]);
    bench_file >> benchmark;
  }

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  if (argc >= 4) tw.app_start_cb(0);
  Qsim::load_file(osd, benchmark.c_str());

  osd.connect_console(std::cout);

  unsigned long inst_per_iter = 1000000000, inst_run;
  // The main loop: run until 'finished' is true.
  inst_run = inst_per_iter;
  while (!tw.hasFinished() && !(inst_per_iter - inst_run)) {
    inst_run = osd.run(inst_per_iter);
  }
  
  if (outfile) { outfile->close(); }
  delete outfile;

  delete osd_p;

  return 0;
}
