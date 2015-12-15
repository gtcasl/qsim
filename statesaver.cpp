/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
// This bit of code is designed to enable saving state in QSim. The primary 
// barrier to successfully saving state is that the program counter available is
// that of the most recent instruction executed, not that of the next
// instruction to be executed. An effective way to get the EIP of the next
// instruction to be executed it to run() it, and using barriers in the
// instruction callbacks, save the state while the callbacks are being called
// and before the instructions have actually been executed.

#include <iostream>
#include <fstream>
#include <vector>
#include <stdio.h>

#include <pthread.h>

#include <qsim.h>

#include "statesaver.h"

class Statesaver;

class Statesaver {
public:
  Statesaver(Qsim::OSDomain &osd, const char* state_filename):
    osd(osd), last_was_br(osd.get_n()), last_was_cbr(osd.get_n())
  {
    Qsim::OSDomain::inst_cb_handle_t icb_handle;
    Qsim::OSDomain::reg_cb_handle_t rcb_handle;

    icb_handle = osd.set_inst_cb(this, &Statesaver::inst_cb);
    rcb_handle = osd.set_reg_cb(this, &Statesaver::reg_cb);

    osd.save_state(state_filename);

    // Unset the callbacks so we can continue.
    osd.unset_inst_cb(icb_handle);
    osd.unset_reg_cb(rcb_handle);
  }

private:
  void inst_cb(int cpu, uint64_t va, uint64_t pa, uint8_t l, const uint8_t *b,
               enum inst_type t);
  void reg_cb(int cpu, int r, uint8_t s, int t);

  Qsim::OSDomain &osd;
  std::vector<bool> last_was_br, last_was_cbr;
};

void Statesaver::inst_cb(int cpu, uint64_t va, uint64_t pa, 
                         uint8_t l, const uint8_t *b, enum inst_type t)
{
  // Wait for the start of a basic block.
  if (last_was_cbr[cpu]) {
    //pthread_barrier_wait(&barrier1);
    //pthread_barrier_wait(&barrier2);
  } else {
    // Handle unconditional branches (jmps)
    last_was_br[cpu] = false;
  }

  if (t == QSIM_INST_BR) {
    last_was_br[cpu] = true;
  }
}

void Statesaver::reg_cb(int cpu, int r, uint8_t s, int t) {
  // If we read flags and are a BR instruction, we're probably conditional.
  if (last_was_br[cpu] && s == 0 && !t) last_was_cbr[cpu] = true;
}

void Qsim::save_state(Qsim::OSDomain &osd, const char *filename) {
  Statesaver ss(osd, filename);
}
