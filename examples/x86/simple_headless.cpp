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

using Qsim::OSDomain;

using std::ostream;

class TraceWriter {
public:
  TraceWriter(OSDomain &osd, ostream &tracefile) : 
    osd(osd), tracefile(tracefile), finished(false) 
  { 
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb); 
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &TraceWriter::inst_cb);
      osd.set_atomic_cb(this, &TraceWriter::atomic_cb);
      osd.set_mem_cb(this, &TraceWriter::mem_cb);
      osd.set_int_cb(this, &TraceWriter::int_cb);
      osd.set_io_cb(this, &TraceWriter::io_cb);
      //osd.set_reg_cb(this, &TraceWriter::reg_cb);
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);

      return 1;
    }

    return 0;
  }

  int app_end_cb(int c)   { finished = true; return 0; }

  int atomic_cb(int c) {
    tracefile << std::dec << c << ": Atomic\n";
    return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
    _DecodedInst inst[15];
    unsigned int shouldBeOne;
    distorm_decode(0, b, l, Decode64Bits, inst, 15, &shouldBeOne);

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

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
    tracefile << std::dec << c << ":  " << (w?"WR":"RD") << "(0x" << std::hex
              << v << "/0x" << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
    return;
  }

  int int_cb(int c, uint8_t v) {
    tracefile << std::dec << c << ": Interrupt 0x" << std::hex << std::setw(2)
              << std::setfill('0') << (unsigned)v << '\n';
    return 0;
  }

  uint32_t *io_cb(int c, uint64_t p, uint8_t s, int w, uint32_t v) {
    tracefile << std::dec << c << ": I/O " << (w?"WR":"RD") << ": (0x" 
              << std::hex << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
    return NULL;
  }

  void reg_cb(int c, int r, uint8_t s, int type) {
    tracefile << std::dec << c << (s == 0?": Flag ":": Reg ") 
              << (type?"WR":"RD") << std::dec;

    if (s != 0) tracefile << ' ' << r << ": " << (unsigned)(s*8) << " bits.\n";
    else tracefile << ": mask=0x" << std::hex << r << '\n';
  }

private:
  OSDomain &osd;
  ostream &tracefile;
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

  // Read trace file as a parameter.
  if (argc >= 3) {
    outfile = new ofstream(argv[2]);
  }

  OSDomain *osd_p(NULL);

  if (argc >= 4) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(argv[3]);
    n_cpus = osd_p->get_n();
  } else {
    osd_p = new OSDomain(n_cpus, qsim_prefix + "/linux/bzImage", "x86", QSIM_HEADLESS);
  }
  OSDomain &osd(*osd_p);

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  if (argc >= 4) tw.app_start_cb(0);

  osd.connect_console(std::cout);

  // The main loop: run until 'finished' is true.
  while (!tw.hasFinished()) {
    for (unsigned i = 0; i < 100; i++) {
           osd.run(0, 10000);
    }
    //osd.timer_interrupt();
  }
  
  if (outfile) { outfile->close(); }
  delete outfile;

  delete osd_p;

  return 0;
}
