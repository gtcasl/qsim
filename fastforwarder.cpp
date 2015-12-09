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
#include <vector>
#include "statesaver.h"

// The following two defines can be used to create instruction traces. DEBUG
// enables tracing and LOAD reconfigures the fastforwarder to load a state
// file and trace this. Doing this allows comparison of the trace of the
// loaded state to the trace of the fastforwarder after loading a state.
//#define LOAD
//#define DEBUG

struct Magic_cb_s {
  Magic_cb_s(Qsim::OSDomain &osd): osd(osd), app_started(false) {}

  Qsim::OSDomain &osd;

  int magic_cb_f(int cpu_id, uint64_t rax) {
    if (rax == 0xaaaaaaaa) {
      app_started = true;
      return 1;
    }
    return 0;
  }
  bool app_started;

  void inst_cb_f(int i, uint64_t p, uint64_t v, uint8_t l, const uint8_t *b,
                 enum inst_type t)
  {
    std::cerr << '\n' << std::dec << i << ' ' << std::hex << p << '/' << v;
  }

  void reg_cb_f(int i, int r, uint8_t s, int w) {
    if (!w && s) {
      std::cerr << ' ' << x86_regs_str[r] << '(' << osd.get_reg(i, regs(r)) << ')';
    } else if (!w) {
      std::cerr << " f" << std::setw(2) << std::setfill('0') << r
                << '(' << std::setw(2) << osd.get_reg(i, QSIM_CPSR) << ')';
    }
  }
};

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cout << "Usage:\n  " << argv[0] 
              << " <bzImage> <# CPUs> <ram size (MB)> <output state file>\n";
    return 1;
  }

  int cpus(atoi(argv[2])), ram_mb(atoi(argv[3]));
  if (cpus <= 0) {
    std::cerr << "# CPUs " << cpus << " out of range.\n"; return 1;
  }

  if (ram_mb < 64) {
    std::cerr << "Ram size " << ram_mb << " out of range.\n"; return 1;
  }

#ifdef LOAD
  Qsim::OSDomain osd("state.debug");
#else
  Qsim::OSDomain osd(cpus, argv[1], "x86", QSIM_HEADLESS, ram_mb);
#endif
  Magic_cb_s magic_cb_s(osd);

  osd.connect_console(std::cout);
#ifndef LOAD
  osd.set_magic_cb(&magic_cb_s, &Magic_cb_s::magic_cb_f);

  std::cout << "Fast forwarding...\n";

  // The thread will be "idle" during initialization. The "slow cycles"
  // mechanism is a dirty hack that keeps timer interrupts from happening
  // before the core is fully booted.
  std::vector<int> slow_cycles(osd.get_n(), 10);
  slow_cycles[0] = 70000;
  do {
    for (unsigned i = 0; i < 100 && !magic_cb_s.app_started; i++) {
      for (int j = 0; j < cpus && !magic_cb_s.app_started; j++) {
        if (osd.runnable(j)) {
          if (osd.idle(j) && !slow_cycles[j]) {
              osd.run(j, 100);
	  } else {
            if (osd.idle(j)) --slow_cycles[j];
            osd.run(j, 10000);
	  }
        }

        // So we don't immediately run the app start callback on load
        if (magic_cb_s.app_started) osd.run(j, 1);
      }
    }
    if (!magic_cb_s.app_started) osd.timer_interrupt();
  } while (!magic_cb_s.app_started);
#endif

#ifdef DEBUG
  osd.set_inst_cb(&magic_cb_s, &Magic_cb_s::inst_cb_f);
  osd.set_reg_cb(&magic_cb_s, &Magic_cb_s::reg_cb_f);
#endif

#ifndef LOAD
  std::cout << "Saving state...\n";
  Qsim::save_state(osd, argv[4]);
#endif

#ifdef DEBUG
  std::cout << "Tracing 1M instructions.\n";
#endif

#ifdef LOAD
  int runfor = 10000;
#else
  int runfor = 9999;
#endif
  for (unsigned i = 0; i < 1; ++i) {
    for (unsigned j = 0; j < 100; ++j) {
      for (int k = 0; k < cpus; ++k) {
        osd.run(k, runfor);
      }
#ifndef LOAD
      runfor = 10000;
#endif
    }
    osd.timer_interrupt();
  }

  std::cout << "Finished.\n";
  
  return 0;
}
