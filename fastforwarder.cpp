/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
// The fast-forwarder: runs until it sees an application start marker and then
// saves the state of the CPUs so it can be reloaded later.
#include <iostream>
#include <sstream>
#include <cstdlib>
#include "statesaver.h"

struct Magic_cb_s {
  int magic_cb_f(int cpu_id, uint64_t rax) {
    if (rax == 0xaaaaaaaa) {
      app_started = true;
      return 1;
    }
    return 0;
  }
  bool app_started;
} magic_cb_s;

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cout <<"Usage:\n  "<<argv[0]<<" <bzImage> <# CPUs> <state file>\n";
    return 1;
  }

  int cpus = atoi(argv[2]);
  if (cpus <= 0) { std::cout << "# CPUs out of range.\n"; return 1; }

  Qsim::OSDomain osd(cpus, argv[1]);

  osd.connect_console(std::cout);
  osd.set_magic_cb(&magic_cb_s, &Magic_cb_s::magic_cb_f);

  std::cout << "Fast forwarding...\n";
  do {
    for (unsigned i = 0; i < 100 && !magic_cb_s.app_started; i++) {
      for (unsigned j = 0; j < cpus && !magic_cb_s.app_started; j++) {
        if (osd.booted(j)) osd.run(j, 10000);

        // So we don't immediately run the app start callback on load
        if (magic_cb_s.app_started) osd.run(j, 1);
      }
    }
    osd.timer_interrupt();
  } while (!magic_cb_s.app_started);

  std::cout << "Saving state...\n";
  Qsim::save_state(osd, argv[3]);

  std::cout << "Finished.\n";
  
  return 0;
}
