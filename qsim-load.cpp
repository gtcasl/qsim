/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <fstream>

#include <qsim.h>

#include "qsim-load.h"


using namespace Qsim;
using namespace std;

class QsimLoadHelper {
public:
  QsimLoadHelper(OSDomain &osd, ifstream &infile):
    osd(osd), infile(infile), finished(false)
  {

    // Set the callbacks.
    OSDomain::magic_cb_handle_t magic_handle(
      osd.set_magic_cb(this, &QsimLoadHelper::magic_cb)
    );

    OSDomain::start_cb_handle_t app_start_handle(
      osd.set_app_start_cb(this, &QsimLoadHelper::app_start_cb)
    );
    
    // The main loop: run until 'finished' is true.                            
    while (!finished) {
      for (unsigned i = 0; i < 1000; i++) {
        for (unsigned j = 0; j < osd.get_n(); j++) {
          osd.run(j, 1000);
        }
        if (finished) break;
      }
      if (!finished) osd.timer_interrupt();
    }

    // Unset the callbacks.
    osd.unset_magic_cb(magic_handle);
    osd.unset_app_start_cb(app_start_handle);
  }

private:
  OSDomain &osd;  
  ifstream &infile;
  bool finished;

  int app_start_cb(int) {
    finished = true;
    return 1;
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
};

void Qsim::load_file(OSDomain &osd, const char *filename) {
  ifstream infile(filename);
  QsimLoadHelper qlh(osd, infile);
  infile.close();
}
