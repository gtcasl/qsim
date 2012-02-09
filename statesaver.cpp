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

#include <pthread.h>

#include <qsim.h>

#include "statesaver.h"

class Statesaver;
struct Statesaver_thread_arg {
  unsigned cpu;
  Statesaver *ss;
  pthread_t pt;
};
void *statesaver_thread_main(void *ss);

class Statesaver {
public:
  Statesaver(Qsim::OSDomain &osd, const char* state_filename): osd(osd) 
  {
    Qsim::OSDomain::inst_cb_handle_t icb_handle;

    pthread_barrier_init(&barrier1, NULL, osd.get_n() + 1);
    pthread_barrier_init(&barrier2, NULL, osd.get_n() + 1);
    icb_handle = osd.set_inst_cb(this, &Statesaver::inst_cb);

    // Spawn n threads, one per CPU.
    unsigned n(osd.get_n());
    std::vector<Statesaver_thread_arg> v(n);
    for (unsigned i = 0; i < n; i++) {
      v[i].ss = this;
      v[i].cpu = i;
      pthread_create(&v[i].pt, NULL, statesaver_thread_main, &v[i]);
    }

    // Between these barriers is the only time our RIP will be up-to-date.
    pthread_barrier_wait(&barrier1);
    osd.save_state(state_filename);
    pthread_barrier_wait(&barrier2);

    // Join the threads.
    for (unsigned i = 0; i < n; i++) pthread_join(v[i].pt, NULL);

    // Unset the callback so we can continue.
    osd.unset_inst_cb(icb_handle);
  }

private:
  void inst_cb(int cpu, uint64_t va, uint64_t pa, uint8_t l, const uint8_t *b,
               enum inst_type t);

  Qsim::OSDomain &osd;
  pthread_barrier_t barrier1, barrier2;
  friend void *statesaver_thread_main(void *ss);
};

void Statesaver::inst_cb(int cpu, uint64_t va, uint64_t pa, 
                         uint8_t l, const uint8_t *b, enum inst_type t)
{
  osd.set_reg(cpu, QSIM_RIP, va);
  pthread_barrier_wait(&barrier1);
  pthread_barrier_wait(&barrier2);
}

void *statesaver_thread_main(void *arg_vp) {
  Statesaver_thread_arg &arg(*(Statesaver_thread_arg*)(arg_vp));
  Statesaver &ss(*arg.ss);

  ss.osd.run(arg.cpu, 1);

  return NULL;
}

void Qsim::save_state(Qsim::OSDomain &osd, const char *filename) {
  Statesaver ss(osd, filename);
}
