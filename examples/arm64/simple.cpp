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
#include <string>
#include <thread>

#include <qsim.h>
#include <qsim-load.h>
#include <capstone.h>

#include "cs_disas.h"

using Qsim::OSDomain;

using std::ostream;

class TraceWriter {
public:
  TraceWriter(OSDomain &osd, ostream &tracefile) : 
	  osd(osd), tracefile(tracefile), finished(false),
	  dis(CS_ARCH_ARM64, CS_MODE_ARM)
  { 
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb);
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &TraceWriter::inst_cb);
      osd.set_mem_cb(this, &TraceWriter::mem_cb);
      //osd.set_int_cb(this, &TraceWriter::int_cb);
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);

      return 1;
    }

    return 0;
  }

  int app_end_cb(int c)   { finished = true; return 0; }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
    cs_insn *insn = NULL;

    int count = dis.decode((unsigned char *)b, l, insn);
    insn[0].address = v;

    tracefile << std::dec << c << ": " << std::hex << insn[0].address << " : "
              << insn[0].mnemonic << " : " << insn[0].op_str << std::endl;

    dis.free_insn(insn, count);
    fflush(NULL);
    return;
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
    tracefile << std::dec << c << ":  " << (w?"WR":"RD") << "(0x" << std::hex
              << v << "/0x" << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
  }

  void reg_cb(int c, int r, uint8_t s, int type) {
    tracefile << std::dec << c << (s == 0?": Flag ":": Reg ") 
              << (type?"WR":"RD") << std::dec;

    if (s != 0) tracefile << ' ' << r << ": " << (unsigned)(s*8) << " bits.\n";
    else tracefile << ": mask=0x" << std::hex << r << '\n';
  }

  int int_cb(int c, uint8_t v) {
    tracefile << std::dec << c << ": Interrupt 0x" << std::hex << std::setw(2)
              << std::setfill('0') << (unsigned)v << '\n';
    return 0;
  }

private:
  OSDomain &osd;
  ostream &tracefile;
  bool finished;
  cs_disas dis;

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
  } else {
    fprintf(stderr, "Usage:\n INTERACTIVE: %s <num_cpus>\n"
            " HEADLESS: %s <num_cpus> -state <state_file> -bench <benchmark.tar> -out <log_file>\n",
            argv[0], argv[0]);
    exit(0);
  }

  // Read trace file as a parameter.
  if (argc >= 7) {
    outfile = new ofstream(argv[7]);
  } else {
    outfile = new ofstream("trace.log");
  }

  OSDomain *osd_p(NULL);

  if (argc >= 4) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(n_cpus, argv[3]);
  } else {
    osd_p = new OSDomain(n_cpus, qsim_prefix + "/images/arm64_images/vmlinuz", "a64", QSIM_INTERACTIVE);
  }
  OSDomain &osd(*osd_p);

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  if (argc >= 6) {
    Qsim::load_file(osd, argv[5]);
    tw.app_start_cb(0);
  }

  osd.connect_console(std::cout);

  tw.app_start_cb(0);
  // The main loop: run until 'finished' is true.
  unsigned long inst_per_iter = 1000000000, inst_run;
  inst_run = inst_per_iter;
  while (!(inst_per_iter - inst_run)) {
    inst_run = 0;
    inst_run = osd.run(inst_per_iter);
    osd.timer_interrupt();
  }
  
  if (outfile) { outfile->close(); }
  delete outfile;

  delete osd_p;

  return 0;
}
