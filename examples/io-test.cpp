/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <iostream>
#include <fstream>
#include <iomanip>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <qsim.h>
#include <qsim-load.h>
#include "distorm.h"

using Qsim::OSDomain;

using std::ostream;

class TraceWriter {
public:
  TraceWriter(OSDomain &osd, ostream &tracefile, const char *in) : 
    osd(osd), tracefile(tracefile), finished(false), infile(in)
  { 
    Qsim::load_file(osd, in);
    std::cout << "Finished loading app.\n";

    osd.set_int_cb(this, &TraceWriter::int_cb);
    osd.set_inst_cb(this, &TraceWriter::inst_cb);
    osd.set_mem_cb(this, &TraceWriter::mem_cb);
    osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
  }

  bool hasFinished() { return finished; }

  int app_end_cb(int c)   { finished = true; return 1; }

  int atomic_cb(int c) {
    tracefile << std::dec << c << ": Atomic\n";
    return 0;
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
    ++memopcount;
    tracefile << std::dec << c << ":   Mem" << (w?"Wr":"Rd") << " 0x"
              << std::hex << v << '(' << std::dec << memopcount << ')'
              << std::endl;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
    _DecodedInst inst[15];
    unsigned int shouldBeOne;
    distorm_decode(0, b, l, Decode32Bits, inst, 15, &shouldBeOne);

    memopcount = 0;

    tracefile << std::dec << c << ": Inst@(0x" << std::hex << v << "/0x" << p 
              << ", tid=" << std::dec << osd.get_tid(c) << ", "
              << ((osd.get_prot(c) == Qsim::OSDomain::PROT_USER)?"USR":"KRN")
              << (osd.idle(c)?"[IDLE]":"[ACTIVE]")
              << "): " << std::hex;

    //while (l--) tracefile << ' ' << std::setw(2) << std::setfill('0') 
    //                      << (unsigned)*(b++);

    if (shouldBeOne != 1) tracefile << "[Decoding Error]";
    else tracefile << inst[0].mnemonic.p << ' ' << inst[0].operands.p;

    tracefile << " (" << itype_str[t] << ")\n";
  }

  int int_cb(int c, uint8_t v) {
    memopcount = 0;
    tracefile << std::dec << c << ": Interrupt 0x" << std::hex << std::setw(2)
              << std::setfill('0') << (unsigned)v << '\n';
    return 0;
  }

  void io_cb(int c, uint64_t p, uint8_t s, int w, uint32_t v) {
    tracefile << std::dec << c << ": I/O " << (w?"RD":"WR") << ": (0x" 
              << std::hex << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
  }

private:
  int memopcount;
  OSDomain &osd;
  ostream &tracefile;
  bool finished;
  char next_char;
  std::ifstream infile;
  static const char *itype_str[];
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

  // Read number of CPUs as a parameter. 
  if (argc >= 2) {
    istringstream s(argv[1]);
    s >> n_cpus;
  }

  // Read trace file as a parameter.
  if (argc >= 3) {
    outfile = new ofstream(argv[2]);
  }

  OSDomain *osd_p(NULL);
  OSDomain &osd(*osd_p);

  if (argc >= 5) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(argv[3]);
    std::cout << "Finished loading state.\n";
    n_cpus = osd.get_n();
  } else {
    std::cout << "Usage:\n  " << argv[0] << " #cpus tracefile statefile tar\n";
    return 1;
  }

  osd.connect_console(std::cout);

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout, argv[4]);

  // The main loop: run until 'finished' is true.
  while (!tw.hasFinished()) {
    for (unsigned i = 0; i < 100; i++) {
      for (unsigned j = 0; j < n_cpus; j++) {
           osd.run(j, 10000);
      }
    }
    osd.timer_interrupt();
  }
  
  if (outfile) { outfile->close(); }
  delete outfile;

  return 0;
}
