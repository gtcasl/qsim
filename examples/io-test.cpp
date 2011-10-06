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

using Qsim::OSDomain;

using std::ostream;

class TraceWriter {
public:
  TraceWriter(OSDomain &osd, ostream &tracefile) : 
    osd(osd), tracefile(tracefile), finished(false), infile("INFILE")
  { 
    osd.set_magic_cb(this, &TraceWriter::magic_cb);
    osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
  }

  bool hasFinished() { return finished; }

  void app_end_cb(int c)   { finished = true; }

  int atomic_cb(int c) {
    tracefile << std::dec << c << ": Atomic\n";
    return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b) {
    tracefile << std::dec << c << ": Inst@(0x" << std::hex << v << "/0x" << p 
              << ", tid=" << std::dec << osd.get_tid(c) << ", "
              << ((osd.get_prot(c) == Qsim::OSDomain::PROT_USER)?"USR":"KRN")
              << (osd.idle(c)?"[IDLE]":"[ACTIVE]")
              << "):" << std::hex;

    while (l--) tracefile << ' ' << std::setw(2) << std::setfill('0') 
                          << (unsigned)*(b++);
    tracefile << '\n';
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
    tracefile << std::dec << c << ":  " << (w?"WR":"RD") << "(0x" << std::hex
              << v << "/0x" << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
  }

  int int_cb(int c, uint8_t v) {
    tracefile << std::dec << c << ": Interrupt 0x" << std::hex << std::setw(2)
              << std::setfill('0') << (unsigned)v << '\n';
    return 0;
  }

  void io_cb(int c, uint64_t p, uint8_t s, int w, uint32_t v) {
    tracefile << std::dec << c << ": I/O " << (w?"RD":"WR") << ": (0x" 
              << std::hex << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
  }

  int magic_cb(int c, uint64_t rax) {
    if (rax == 0xc5b1fffd) {
      // Giving an address to deposit 1024 bytes in %rbx. Wants number of bytes
      // actually deposited in %rcx.
      uint64_t vaddr = osd.get_reg(c, QSIM_RBX);
      int count = 1024;
      while (infile.good() && count) {
        char ch;
        infile.get(ch);
        osd.mem_wr_virt(c, ch, vaddr++);
        count--;
      }
      osd.set_reg(c, QSIM_RCX, 1024-count);
    } else if (rax == 0xc5b1fffe) {
      // Asking if input is ready
      osd.set_reg(c, QSIM_RAX, !(!infile));
    } else if (rax == 0xc5b1ffff) {
      // Asking for a byte of input.
      char ch;
      infile.get(ch);
      osd.set_reg(c, QSIM_RAX, ch);
    } else if (rax&0xffffff00 == 0xc5b100) {
      std::cout << "binary write: " << (rax&0xff) << '\n';
    }

    return 0;
  }

private:
  OSDomain &osd;
  ostream &tracefile;
  bool finished;
  char next_char;
  std::ifstream infile;
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

  if (argc >= 4) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(argv[3]);
    n_cpus = osd.get_n();
  } else {
    osd_p = new OSDomain(n_cpus, "linux/bzImage");
  }

  osd.connect_console(std::cout);

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout);

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
