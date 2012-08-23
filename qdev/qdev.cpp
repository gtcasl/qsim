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
#include <string>

#include <qsim.h>

#include <distorm.h>

#define RAM_MB 128

using Qsim::OSDomain;
#define QSIM_OBJECT OSDomain

using std::ostream;

class TraceWriter {
public:
  TraceWriter(QSIM_OBJECT &osd, ostream &tracefile) : 
    osd(osd), tracefile(tracefile), finished(false), call(false), callLvl(0)
  { 
    readSyms("../linux64/linux-3.5.2/System.map");

    //osd.set_app_start_cb(this, &TraceWriter::app_start_cb); 
    app_start_cb(0);
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
      osd.set_reg_cb(this, &TraceWriter::reg_cb);
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);

      // Automatically return from bogus BIOS calls.
      { uint8_t x(0xcf); osd.mem_wr(x,0); }

      return 1;
    }
  }

  int app_end_cb(int c)   { finished = true; return 0; }

  int atomic_cb(int c) {
    tracefile << std::dec << c << ": Atomic\n";
    return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
    if (call) {
      std::cout << "Call: ";
      for (unsigned i = 0; i < callLvl; ++i) { std::cout << ' '; }
      ++callLvl;
      if (syms.find(v) != syms.end())  {
	std::cout << syms[v] << '\n';
      } else {
	std::cout << "[unknown]\n";
      }
      call = false;
    }

#if 0
    unsigned shouldBeOne;
    _DecodedInst inst[15];
    distorm_decode(0, b, l, v==p?Decode16Bits:Decode64Bits, inst, 15,
                   &shouldBeOne);

    tracefile << std::dec << c << ": Inst@(0x" << std::hex << v << "/0x" << p
              << ", tid=" << std::dec << osd.get_tid(c) << ", "
              << ((osd.get_prot(c) == Qsim::OSDomain::PROT_USER)?"USR":"KRN")
              << (osd.idle(c)?"[IDLE])":"[ACTIVE])")
              << inst[0].mnemonic.p << ' ' << inst[0].operands.p << '\n';
#endif

    iAddr = v;

    if (t == QSIM_INST_CALL) {
      // Call instruction
      call = true;
    } else if (t == QSIM_INST_RET && !(l == 1 && *b == 0xcf)) {
      // Ret instruction
      if (callLvl > 0) --callLvl;
    } else if (t == QSIM_INST_RET) {
      // reti instruction
      callLvl = sCallLvl;
    }
  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
#if 0
    tracefile << "0x" << std::hex << iAddr << ": " << std::dec << c << ": MEM "
              << (w?"WR":"RD") << "(0x" << std::hex << v << "/0x" << p
              << "): " << std::dec << (unsigned)(s*8) << " bits.\n";
#endif

    if (p >= 0xa0000 && p <= 0xbffff) {
      char c;
      osd.mem_rd(c, p);

      std::cout << "Wrote '" << c << "' to vram.\n";

      if (c == '\n') {
	std::cout << "VGA: " << linebuf;
        linebuf = "";
      } else {
	linebuf += c;
      }
    }
  }

  int int_cb(int c, uint8_t v) {
    tracefile << std::dec << c << ": Interrupt 0x" << std::hex << std::setw(2)
              << std::setfill('0') << (unsigned)v << " A="
              << osd.get_reg(c, QSIM_RAX) << " C=" << osd.get_reg(c, QSIM_RCX)
              << " B=" << osd.get_reg(c, QSIM_RBX) << " D="
              << osd.get_reg(c, QSIM_RDX) << '\n';

    sCallLvl = callLvl;
    callLvl = 0;

    if (v == 0x10) {
      uint64_t rax(osd.get_reg(c, QSIM_RAX));
    
      switch(rax>>8) {
      case 0x00: { // Set video mode
        // Just ignore this.
        break;
      }

      case 0x03: { // Get cursor position and size
        osd.set_reg(c, QSIM_RCX, 0xffff); // "No cursor."
        osd.set_reg(c, QSIM_RDX, 0x0000);
        break;
      }

      case 0x0e: { // Print character
        char c(rax&0xff);
        if (c == '\n') {
	  std::cout << "Console: " << linebuf << '\n';
          linebuf = "";
        } else {
          linebuf += (char)(rax&0xff);
        }
        break;
      }

      case 0x0f: { // Get current video mode
        osd.set_reg(c, QSIM_RAX, 0x5003); // 80 column VGA.
        osd.set_reg(c, QSIM_RBX, 0x0500);
        break;
      }
      
      case 0x12: { // Video "alternate function select"
        switch(osd.get_reg(c, QSIM_RBX)&0xff) {
        case 0x10: { // Get EGA info.
          osd.set_reg(c, QSIM_RBX, 0x0003);
          osd.set_reg(c, QSIM_RCX, 0x00f0);
          break;
	}

	default: {
	  std::cout << "Unsupported video \"alternate function\" BIOS call.\n";
          exit(1);
        }
	}
        break;
      }

      case 0x1a: { // Get display combination code.
        osd.set_reg(c, QSIM_RAX, 0x1a1a);
        osd.set_reg(c, QSIM_RBX, 0x0008);
        break;
      }

      case 0x4f: { // Get SuperVGA information
        osd.set_reg(c, QSIM_RAX, 0x0000); // Unsupported.
        break;
      }

      default:
        std::cout << "Unknown BIOS call.\n";
        exit(1);
      }
    } else if (v == 0x15) {
      uint64_t rax(osd.get_reg(c, QSIM_RAX));
     
      switch(rax>>8) {
      case 0xc0: {
        uint64_t addr((osd.get_reg(c, QSIM_ES)<<4)+osd.get_reg(c, QSIM_RBX));

        uint16_t sz(7);
        uint8_t x[7] = {0x95, 0x13, 0x00, 0x60, 0x80, 0x00, 0x00};
        osd.mem_wr(sz, addr);
        for (unsigned i = 0; i < 7; ++i) osd.mem_wr(x[i], addr+2+i);

        // Clear CF, meaning "success".
	osd.set_reg(c, QSIM_RFLAGS, osd.get_reg(c, QSIM_RFLAGS) & 0xfffe);
        osd.set_reg(c, QSIM_RAX, osd.get_reg(c, QSIM_RAX) & 0x00ff);
        break;
      }
      case 0xec: /*This just tells the BIOS which mode we're using.*/ break;
      case 0x88: {
        // Return number of contiguous KB above 1M, up to 15MB
        uint16_t x((RAM_MB > 16?15:(RAM_MB-1))*1024);
        osd.set_reg(c, QSIM_RAX, x);
        break;
      }

      case 0xe9: {
        if (rax == 0xe980) {
          // Set the carry flag: speedstep is unsupported.
          osd.set_reg(c, QSIM_RFLAGS, osd.get_reg(c, QSIM_RFLAGS) | 1);
          break;
        }
      }

      case 0xe8: {
        if (rax == 0xe820) {
          uint64_t addr((osd.get_reg(c, QSIM_ES)<<4)+osd.get_reg(c, QSIM_RDI));

          uint64_t base(0), len(RAM_MB<<20);
          uint32_t type(1);

          osd.mem_wr(base, addr);
          osd.mem_wr(len, addr+8);
          osd.mem_wr(type, addr+16);

          // Make RBX 0. This is the last entry.
          osd.set_reg(c, QSIM_RFLAGS, osd.get_reg(c, QSIM_RFLAGS) & 0xfffe);
          osd.set_reg(c, QSIM_RBX, 0);

          // Number of bytes returned.
          osd.set_reg(c, QSIM_RCX, 0x20);
          break;
	} else if (rax == 0xe801) {
          uint16_t x1(RAM_MB<16?(RAM_MB-1)*1024:0x3c00),
  	           x2(RAM_MB<4096?RAM_MB*16:0xff00);

          osd.set_reg(c, QSIM_RAX, x1);
          osd.set_reg(c, QSIM_RCX, x1);
          osd.set_reg(c, QSIM_RBX, x2);
          osd.set_reg(c, QSIM_RDX, x2);

          osd.set_reg(c, QSIM_RFLAGS, osd.get_reg(c, QSIM_RFLAGS) & 0xfffe);
          break;
        }
      }
      default: {
        std::cout << "Unknown BIOS call.\n";
        exit(1);
      }
      }
    }

    return 0;
  }

  void io_cb(int c, uint64_t p, uint8_t s, int w, uint32_t v) {
    tracefile << "0x" << std::hex << iAddr << ": " << std::dec << c
              << ": I/O " << (w?"WR":"RD") << ": (0x" << std::hex << p
              << "): " << std::dec << (unsigned)(s*8) << " bits.\n";

    if (p >= 0x20 && p <= 0xbf || p >= 0xa0 && p <= 0xbf) { // 8259
      bool slave(p & 0x80);
    } else if (p == 0x70) { // CMOS register select, NMI enable
      if (w) {
        cmosReg = v & 0x7f;
      } else {
	std::cout << "IN unsupported.\n";
        exit(1);
      }
    } else if (p == 0x80) { // Dummy port
      if (!w) {
	std::cout << "Tried to read from port 0x80.\n";
        exit(1);
      }
    } else if (p >= 0xf0 && p <= 0xff) { // FPU control
      // Nothing to do here.
    } else if (p >= 0x3c0 && p <= 0x3df) { // VGA registers
      // Ignore these.
    } else if (p >= 0xcf8 && p <= 0xcff) { // PCI CONFIG_ADDRESS, CONFIG_DATA
      // PCI configura
    }else {
      std::cout << "Unsupported IO port address.\n";
      exit(1);
    }
  }

  void reg_cb(int c, int r, uint8_t s, int type) {
#if 0
    tracefile << std::dec << c << (s == 0?": Flag ":": Reg ") 
              << (type?"WR":"RD") << std::dec;

    if (s != 0) tracefile << ' ' << r << ": " << (unsigned)(s*8) << " bits.\n";
    else tracefile << ": mask=0x" << std::hex << r << '\n';
#endif
  }

private:
  QSIM_OBJECT &osd;
  ostream &tracefile;
  bool finished;
  uint64_t iAddr;
  std::string linebuf;

  uint8_t cmosReg;

  std::map<uint64_t, std::string> syms;
  int callLvl, sCallLvl;
  bool call;

  void readSyms(const char* filename) {
    std::ifstream f(filename);

    uint64_t addr; char c; std::string sym;
    while (!!f) {
      f >> std::hex >> addr >> c >> sym; syms[addr] = sym;
      std::cout << std::hex << addr << ' ' << c << ' ' << sym << '\n';
    }
  }
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
    osd_p = new OSDomain(n_cpus, "../linux64/bzImage", RAM_MB);
  }

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout);

  osd.connect_console(std::cout);

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

  delete osd_p;

  return 0;
}
