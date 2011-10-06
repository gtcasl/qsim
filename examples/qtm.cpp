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

#include "distorm.h"
#include "qsim.h"

using std::cout; using std::vector; using std::ofstream; using std::string;
using Qsim::OSDomain; using std::map;

const unsigned BRS_PER_MILN  = 1            ;
const unsigned MILLION_INSTS = 48000        ;
const unsigned N_CPUS        = 16           ;

pthread_mutex_t   output_mutex      = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t cpu_barrier1;
pthread_barrier_t cpu_barrier2;

ofstream tout;

struct thread_arg_t {
  int       cpu   ;
  OSDomain  *cd    ;
  uint64_t  icount;
};

vector<thread_arg_t*> thread_args(N_CPUS);
vector<pthread_t   *> threads    (N_CPUS);

void *cpu_thread_main(void* thread_arg) {
  void mem_cb(int cpu, uint64_t vaddr, uint64_t paddr, uint8_t size, int type);
  thread_arg_t *arg = (thread_arg_t*)thread_arg;
  uint64_t local_inst_count = 0;
  
  // The first barrier. Wait for all of the threads to get initialized.
  pthread_barrier_wait(&cpu_barrier1);

  if (arg->cpu == 0) cout << "QTM threads ready.\n";

  // Outer loop: run for MILLION_INSTS million instructions
  for (unsigned i = 0; i < MILLION_INSTS * BRS_PER_MILN; i++) {
    unsigned countdown = 1000000/BRS_PER_MILN;
    while (countdown > 0) {
      int rval = arg->cd->run(arg->cpu, countdown);
      
      local_inst_count += rval;
      countdown -= rval;
      if (arg->icount != local_inst_count) {
        cout << '(' << local_inst_count << '/' 
             << arg->icount << ")\n"; 
      }

      uint64_t last_rip = arg->cd->get_reg(arg->cpu, QSIM_RIP);
      uint16_t last_tid = arg->cd->get_tid(arg->cpu);
      bool     kernel   = arg->cd->get_prot(arg->cpu) == OSDomain::PROT_KERN;
      if (rval != 0) {
        pthread_mutex_lock(&output_mutex);
        tout << "Ran CPU " <<std::dec<<arg->cpu<<" for "<< rval << " insts,"
         " stopping at 0x" << std::hex << std::setfill('0') << std::setw(8)
               << last_rip << "(TID=" << std::dec << last_tid << ')' 
               << (kernel?"-kernel\n":"\n");
        pthread_mutex_unlock(&output_mutex);
      }

      // This CPU may not be running yet. Get out and wait for the barrier.
      if (countdown == 1000000/BRS_PER_MILN) break;
    }
    // We call the timer interrupt in one thread, while no others are running.
    pthread_barrier_wait(&cpu_barrier1);
    if (arg->cpu == 0) {
      if (i % BRS_PER_MILN == (BRS_PER_MILN - 1)) {
	arg->cd->timer_interrupt();
	//cout << "---Timer Interrupt---\n";
      }
    }
    pthread_barrier_wait(&cpu_barrier2);
  }

  return NULL;
}

void inst_cb(int            cpu_id, 
	     uint64_t       vaddr,
	     uint64_t       paddr, 
	     uint8_t        len, 
	     const uint8_t *bytes,
             enum inst_type type) 
{
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);

  thread_args[cpu_id]->icount++;

#if 0
  if (thread_args[cpu_id]->cd->get_prot(cpu_id) == OSDomain::PROT_KERN) {
    kernel_inst_counts[tid]++;
  } else {
    user_inst_counts[tid]++;
  }
#endif

  if (len == 1 && *bytes == 0xf4) {
    thread_args[cpu_id]->cd->timer_interrupt();
  }
}

int int_cb(int cpu_id, uint8_t vec) 
{
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);

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

int main(int argc, char** argv) {
  // Open trace file.
  tout.open("EXEC_TRACE");
  if (!tout) { cout << "Could not open EXEC_TRACE for writing.\n"; exit(1); }

  // Init. sync objects
  pthread_barrier_init(&cpu_barrier1, NULL, N_CPUS);
  pthread_barrier_init(&cpu_barrier2, NULL, N_CPUS);

  // Create a runnable OSDomain.
  OSDomain cd(N_CPUS, "linux/bzImage", 3*1024);
  cd.set_inst_cb    (inst_cb  );
  cd.set_int_cb     (int_cb   );
  cd.set_mem_cb     (NULL     );
  cd.connect_console(cout     );

  // Launch threads
  for (unsigned i = 0; i < N_CPUS; i++) {
    threads[i]     = new pthread_t    ;
    thread_args[i] = new thread_arg_t ;

    thread_args[i]->cpu    = i;
    thread_args[i]->cd     = &cd;
    thread_args[i]->icount = 0;

    pthread_create(threads[i], NULL, cpu_thread_main, thread_args[i]);
  }

  // Wait for threads to end
  for (unsigned i = 0; i < N_CPUS; i++) pthread_join(*threads[i], NULL);

  // Print stats.
  for (int i = 0; i < N_CPUS; i++) {
    cout << "CPU " << i << ": " << thread_args[i]->icount 
         << " instructions.\n";
  }

  // Clean up.
  pthread_barrier_destroy(&cpu_barrier1);
  pthread_barrier_destroy(&cpu_barrier2);
  for (unsigned i = 0; i < N_CPUS; i++) { 
    delete threads[i]; 
    delete thread_args[i];
  }

  tout.close();

  return 0;
}
