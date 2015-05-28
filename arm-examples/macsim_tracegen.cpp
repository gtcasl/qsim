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
#include <string>
#include <thread>

#include <qsim.h>
#include <stdio.h>

#include "gzstream.h"

using Qsim::OSDomain;

using std::ostream;

class TraceWriter {
public:
  TraceWriter(OSDomain &osd) :
    osd(osd), finished(false)
  { 
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb);
    trace_file_count = 0;
    finished = false;
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &TraceWriter::inst_cb);
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
    }
    tracefile = new ogzstream(("trace_" + std::to_string(trace_file_count) + ".log.gz").c_str());
    trace_file_count++;
    finished = false;

    return 0;
  }

  int app_end_cb(int c)
  {
      std::cout << "App end cb called" << std::endl;
      finished = true;

      if (tracefile) {
          tracefile->close();
          delete tracefile;
          tracefile = NULL;
      }

      return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
      if (tracefile)
          *tracefile << std::dec << c << ": " << std::hex << v << " "
                                       << std::hex << p << " "
                                       << std::hex << l << " "
                                       << std::hex << b << " "
                                       << t
                                       << std::endl;
      else
          std::cout << "Writing to a null tracefile" << std::endl;
    fflush(NULL);
    return;
  }

private:
  OSDomain &osd;
  ogzstream* tracefile;
  bool finished;
  int  trace_file_count;

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

  unsigned n_cpus = 1;

  std::string qsim_prefix(getenv("QSIM_PREFIX"));

  // Read number of CPUs as a parameter. 
  if (argc >= 2) {
    istringstream s(argv[1]);
    s >> n_cpus;
  }

  OSDomain *osd_p(NULL);

  if (argc >= 4) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(argv[3]);
    n_cpus = osd_p->get_n();
  } else {
    osd_p = new OSDomain(n_cpus, qsim_prefix + "/../arm64_images/vmlinuz");
  }
  OSDomain &osd(*osd_p);

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  //if (argc >= 4) tw.app_start_cb(0);

  osd.connect_console(std::cout);

  //tw.app_start_cb(0);
  // The main loop: run until 'finished' is true.
  uint64_t inst_per_iter = 1000000000;
  int inst_run = inst_per_iter;
  while (!(inst_per_iter - inst_run)) {
    for (unsigned long j = 0; j < n_cpus; j++) {
        inst_run = osd.run(j, inst_per_iter);
    }
    osd.timer_interrupt();
  }

  delete osd_p;

  return 0;
}
