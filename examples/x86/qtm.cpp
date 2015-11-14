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
#include <vector>
#include <string>
#include <map>

#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include <sys/time.h>

#include "distorm.h"
#include <qsim.h>
#include <qsim-load.h>

using std::cout; using std::vector; using std::ofstream; using std::string;
using Qsim::OSDomain; using std::map;

const unsigned BRS_PER_MILN = 1  ;
const unsigned MAX_CPUS     = 16;

pthread_mutex_t   output_mutex      = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t cpu_barrier1;
pthread_barrier_t cpu_barrier2;

ofstream tout;

struct thread_arg_t {
  int       cpu   ;
  OSDomain  *cd    ;
  uint64_t  icount;
};

vector<thread_arg_t*> thread_args(MAX_CPUS);
vector<pthread_t   *> threads    (MAX_CPUS);

static inline unsigned long long utime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000l*tv.tv_sec + tv.tv_usec;
}

bool app_finished = false, do_exit = false;
unsigned long long start_time, end_time;

void *cpu_thread_main(void* thread_arg) {
  void mem_cb(int cpu, uint64_t vaddr, uint64_t paddr, uint8_t size, int type);
  thread_arg_t *arg = (thread_arg_t*)thread_arg;
  
  // The first barrier. Wait for all of the threads to get initialized.
  pthread_barrier_wait(&cpu_barrier1);

  if (arg->cpu == 0) cout << "QTM threads ready.\n";

  // Run until app finished.
  unsigned i = 0;
  while (!do_exit)
  {
    arg->cd->run(arg->cpu, 1000000/BRS_PER_MILN);
    arg->icount += 1000000/BRS_PER_MILN;
      
    uint64_t last_rip = arg->cd->get_reg(arg->cpu, QSIM_RIP);
    uint16_t last_tid = arg->cd->get_tid(arg->cpu);
    bool     kernel   = arg->cd->get_prot(arg->cpu) == OSDomain::PROT_KERN;
    if (i % BRS_PER_MILN == (BRS_PER_MILN - 1)) {
      pthread_mutex_lock(&output_mutex);
      tout << "Ran CPU " <<std::dec<<arg->cpu<<" for "<< 1000000/BRS_PER_MILN
           << " insts, stopping at 0x" << std::hex << std::setfill('0') 
           << std::setw(8) << last_rip << "(TID=" << std::dec << last_tid
           << ')' << (kernel?"-kernel\n":"\n");
      tout << std::hex << arg->cd->get_reg(arg->cpu, QSIM_RAX) << ", "
           << std::hex << arg->cd->get_reg(arg->cpu, QSIM_RCX) << ", "
           << std::hex << arg->cd->get_reg(arg->cpu, QSIM_RBX) << ", "
           << std::hex << arg->cd->get_reg(arg->cpu, QSIM_RDX) << '\n';
      pthread_mutex_unlock(&output_mutex);
    }

    // We call the timer interrupt in one thread, while no others are running.
    pthread_barrier_wait(&cpu_barrier1);
    if (arg->cpu == 0) {
      if (i % BRS_PER_MILN == (BRS_PER_MILN - 1)) {
	arg->cd->timer_interrupt();
      }

      if (app_finished) do_exit = true;
    }
    pthread_barrier_wait(&cpu_barrier2);
    ++i;
  }

  return NULL;
}
struct cb_struct {
void inst_cb(int            cpu_id, 
	     uint64_t       vaddr,
	     uint64_t       paddr, 
	     uint8_t        len, 
	     const uint8_t *bytes,
             enum inst_type type) 
{
#if 0
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);

  if (thread_args[cpu_id]->cd->get_prot(cpu_id) == OSDomain::PROT_KERN) {
    kernel_inst_counts[tid]++;
  } else {
    user_inst_counts[tid]++;
  }
#endif
}

int int_cb(int cpu_id, uint8_t vec) 
{
  pthread_mutex_lock(&output_mutex);
  tout << "CPU " << cpu_id << ": Interrupt 0x" << std::hex 
       << std::setw(2) << (unsigned)vec << '\n';
  pthread_mutex_unlock(&output_mutex);

  if (vec != 0xef && vec != 0x30 && vec != 0x80 && vec != 0x0e) {
    //pthread_mutex_lock(&output_mutex);
    //cout << "Interrupt 0x" << std::hex << (unsigned)vec << " CPU " << std::dec 
    //     << cpu_id << '\n';
    //pthread_mutex_unlock(&output_mutex);
  }

  if (vec == 0x0e) {
#if 0
    uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);
    pagefault_counts[tid]++;
#endif
  }

  return 0;
}

void mem_cb(int cpu_id, uint64_t vaddr, uint64_t paddr, uint8_t size, int type)
{
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);

  pthread_mutex_lock(&output_mutex);
  tout << "CPU " << std::dec << cpu_id << ": mem op at 0x" << std::hex 
       << std::setw(8) << paddr << (type?"(R)":"(W)") << " TID " 
       << std::dec << tid << '\n';
  pthread_mutex_unlock(&output_mutex);
}

int end_cb(int c) { app_finished = true; return 1; }

} cb_obj;

int main(int argc, char** argv) {
  // Open trace file.
  tout.open("EXEC_TRACE");
  std::string qsim_prefix(getenv("QSIM_PREFIX"));
  if (!tout) { cout << "Could not open EXEC_TRACE for writing.\n"; exit(1); }

  // Create a runnable OSDomain.
  OSDomain *cdp = NULL;
  OSDomain &cd(*cdp);
  if (argc < 3) {
    cdp = new OSDomain(MAX_CPUS, qsim_prefix + "/../x86_64_images/vmlinuz", "x86");
    //cd.connect_console(cout);
  } else {
    cdp = new OSDomain(argv[1]);
    //cd.connect_console(cout);
    cout << "Loaded state. Reading benchmark.\n";
    Qsim::load_file(cd, argv[2]);
    cout << "Loaded benchmark .tar file.\n";
  }
  cd.set_inst_cb(&cb_obj, &cb_struct::inst_cb);
  cd.set_int_cb(&cb_obj, &cb_struct::int_cb);
  cd.set_app_end_cb(&cb_obj, &cb_struct::end_cb);

  // Init. sync objects 
  pthread_barrier_init(&cpu_barrier1, NULL, cd.get_n());
  pthread_barrier_init(&cpu_barrier2, NULL, cd.get_n());


  // Launch threads
  start_time = utime();
  for (int i = 0; i < cd.get_n(); i++) {
    threads[i]     = new pthread_t    ;
    thread_args[i] = new thread_arg_t ;

    thread_args[i]->cpu    = i;
    thread_args[i]->cd     = &cd;
    thread_args[i]->icount = 0;

    pthread_create(threads[i], NULL, cpu_thread_main, thread_args[i]);
  }

  // Wait for threads to end
  for (int i = 0; i < cd.get_n(); i++) pthread_join(*threads[i], NULL);
  end_time = utime();

  // Print stats.
  for (int i = 0; i < cd.get_n(); i++) {
    cout << "CPU " << i << ": " << thread_args[i]->icount 
         << " instructions.\n";
  }
  cout << end_time - start_time << "us\n";

  // Clean up.
  pthread_barrier_destroy(&cpu_barrier1);
  pthread_barrier_destroy(&cpu_barrier2);
  for (int i = 0; i < cd.get_n(); i++) { 
    delete threads[i]; 
    delete thread_args[i];
  }

  tout.close();

  return 0;
}
