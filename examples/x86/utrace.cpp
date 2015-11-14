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
#include <vector>

#include "distorm.h"

#include <qsim.h>
#include <qsim-load.h>

#ifdef QSIM_REMOTE
#include "../remote/client/qsim-client.h"
using Qsim::Client;
#define QSIM_OBJECT Client
#else
using Qsim::OSDomain;
using Qsim::load_file;
#define QSIM_OBJECT OSDomain
#endif

using std::ostream;

const char *optype_str[] = {
  "null", "intbasic", "intmul", "intdiv", "stack", "br", "call", "ret", "trap",
  "fpbasic", "fpmul", "fpdiv"
};

struct UOp {
  unsigned cpu;
  enum inst_type type;
  uint64_t sr, dr, sf, df;
  bool mem, wr, lock, final, bt;
  uint64_t vaddr, paddr, mem_vaddr, mem_paddr;
  uint8_t ilen;
};

bool first_uop(true);
UOp cur_uop;

static void print_header(std::ostream &os) {
  os << "CPU, Inst. Virt. Addr., Inst. Phys. Addr., Op. Type, Final, "
        "Src. Regs, Dest. Regs, Src. Flags, Dest. Flags, Mem, MemWr, "
        "Data Virt. Addr., Data Phys. Addr., Lock, Branch Taken\n";
}

static std::ostream &operator<<(std::ostream &os, const UOp &uop) {
  os << uop.cpu << ", " << uop.vaddr << ", " << uop.paddr << ", "
     << optype_str[uop.type] << (uop.final?", 1,":", 0,");
  for (unsigned i = 0; i < 64; ++i) if (uop.sr & (1ull<<i)) os << ' ' << i;
  os << ',';
  for (unsigned i = 0; i < 64; ++i) if (uop.dr & (1ull<<i)) os << ' ' << i;
  os << ',';
  for (unsigned i = 0; i < 64; ++i) if (uop.sf & (1ull<<i)) os << ' ' << i;
  os << ',';
  for (unsigned i = 0; i < 64; ++i) if (uop.df & (1ull<<i)) os << ' ' << i;

  os << (uop.mem?", 1":", 0") << (uop.wr?", 1":", 0")
     << ", " << uop.mem_vaddr << ", " << uop.mem_paddr << ", "
     << (uop.lock?'1':'0') << ", " << (uop.bt?'1':'0') << '\n';

  return os;
}

class TraceWriter {
public:
  TraceWriter(QSIM_OBJECT &osd, ostream &tracefile) : 
    osd(osd), tracefile(tracefile), finished(false),
    icount(osd.get_n()), uopcount(osd.get_n()), brtaken(osd.get_n()),
    brnottaken(osd.get_n()) 
  { 
#ifdef QSIM_REMOTE
    app_start_cb(0);
#else
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb); 
#endif
  }

  ~TraceWriter() {
    std::cout << "Per-CPU stats:\n" 
                 "Instructions, uOps, Taken Branches, Not-taken Branches\n";
    for (unsigned i = 0; i < osd.get_n(); ++i) {
      std::cout << icount[i] << ", " << uopcount[i] << ", " 
                << brtaken[i] << ", " << brnottaken[i] << '\n';
    }
  }

  bool hasFinished() { return finished; }

  void app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &TraceWriter::inst_cb);
      osd.set_atomic_cb(this, &TraceWriter::atomic_cb);
      osd.set_mem_cb(this, &TraceWriter::mem_cb);
      osd.set_int_cb(this, &TraceWriter::int_cb);
      osd.set_io_cb(this, &TraceWriter::io_cb);
      osd.set_reg_cb(this, &TraceWriter::reg_cb);
#ifndef QSIM_REMOTE
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
#endif
    }
  }

  void app_end_cb(int c)   { finished = true; }

  int atomic_cb(int c) {
    cur_uop.lock = 1;
    return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
    if (first_uop) {
      first_uop = false;
    } else {
      cur_uop.bt = (cur_uop.type == QSIM_INST_BR && 
                    v != cur_uop.vaddr + cur_uop.ilen);
      if (cur_uop.bt) ++brtaken[c];
      else if (cur_uop.type == QSIM_INST_BR) ++ brnottaken[c];
      tracefile << cur_uop;
      cur_uop = UOp();
    }

    cur_uop.cpu = c;
    cur_uop.type = t;
    cur_uop.final = true;
    cur_uop.vaddr = v;
    cur_uop.paddr = p;
    cur_uop.ilen = l;

    ++icount[c];
    ++uopcount[c];
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
    // If our micro-op already accesses memory, we need to create a new one.
    if (cur_uop.mem) {
      cur_uop.dr |= (1ull<<63);
      cur_uop.final = false;
      tracefile << cur_uop;
      ++uopcount[c];
      cur_uop.final = true;
      cur_uop.sr = (1ull<<63);
      cur_uop.dr = cur_uop.sf = cur_uop.df = 0;
      cur_uop.type = QSIM_INST_NULL;
    }

    cur_uop.mem = true;
    cur_uop.wr = w;
    cur_uop.mem_vaddr = v;
    cur_uop.mem_paddr = p;
  }

  int int_cb(int c, uint8_t v) {
    return 0;
  }

  void io_cb(int c, uint64_t p, uint8_t s, int w, uint32_t v) {
  }

  void reg_cb(int c, int r, uint8_t s, int type) {
    // Flags
    if (s == 0) (type?cur_uop.df:cur_uop.sf) |= r;
    // Registers
    else        (type?cur_uop.dr:cur_uop.sr) |= (1<<r);
  }

private:
  QSIM_OBJECT &osd;
  ostream &tracefile;
  bool finished;

  std::vector<uint64_t> icount, uopcount, brtaken, brnottaken;
};

int main(int argc, char** argv) {
  using std::istringstream;
  using std::ofstream;

  ofstream *outfile(NULL);

  unsigned n_cpus = 1;

  if (argc == 1) {
    std::cout << "Usage:\n  " << argv[0] << " <# CPUs> "
                 "[[[trace] state] benchmark.tar]\n";
    return 0;
  }

#ifndef QSIM_REMOTE
  // Read number of CPUs as a parameter. 
  if (argc >= 2) {
    istringstream s(argv[1]);
    s >> n_cpus;
  }
#endif

  // Read trace file as a parameter.
  if (argc >= 3) {
    outfile = new ofstream(argv[2]);
  }

#ifdef QSIM_REMOTE
  Client osd(client_socket("localhost", "1234"));
  n_cpus = osd.get_n();
#else
  OSDomain *osd_p(NULL);
  OSDomain &osd(*osd_p);

  if (argc >= 4) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(argv[3]);
    n_cpus = osd.get_n();
  } else {
    osd_p = new OSDomain(n_cpus, "linux/bzImage");
  }

  if (argc >= 5) {
    load_file(osd, argv[4]);
  }
#endif

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  if (argc >= 4) tw.app_start_cb(0);

#ifndef QSIM_REMOTE
  osd.connect_console(std::cout);
#endif

  print_header(outfile?*outfile:std::cout);

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
#ifndef QSIM_REMOTE
  delete osd_p;
#endif

  return 0;
}
