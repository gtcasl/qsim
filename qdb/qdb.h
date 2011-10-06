/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#ifndef __QDB_H
#define __QDB_H
#include <string>
#include <map>
#include <list>
#include <vector>
#include <qsim.h>
#include <pthread.h>

// Forward declarations
class CPUThread;

// Shared global variables
extern Qsim::OSDomain *cd;
extern std::vector<CPUThread *> thread_objs;
extern unsigned                 n_cpus;
extern unsigned                 barriers_per_interrupt;
// Symbol table manipulation
void load_symbols (std::string filename, bool global, uint64_t cr3);
void clear_symbols(                      bool global, uint64_t cr3);

// Helper functions
void cpu_stat(unsigned i);
void do_report();
void prof_all();
void prof_on(int tid);
void prof_off(int tid);
void prof_off();
void mem_dump(uint64_t paddr, uint64_t size);
void disas(uint64_t vaddr, uint64_t size);

// Class representing the CPU thread.
class CPUThread {
 public:
  CPUThread(Qsim::OSDomain *cd, unsigned cpu_id);
  ~CPUThread();

  // Control the CPU
  void run(uint64_t insts);        // Nonblocking call: run the CPU.
  void ctrl_c  ();                 // Cancel current instruction run.
  void set_interval(uint64_t);     // Set interval to a new value.

  // QSIM Callbacks
  static int atomic_callback(int cpu_id);

  // CPU Progress Synchronization (initialized in main())
  static pthread_barrier_t before_int;
  static pthread_barrier_t after_int;

 private:
  pthread_t thread;
  static std::map <unsigned, CPUThread*> directory;

  static void *thread_main(void* arg);
  
  pthread_mutex_t countdown_mutex; // Keep the countdown from getting weird.
  pthread_cond_t  wake_up_thread;  // Wake up the sleeping barber.
  uint64_t countdown;  // Number of instructions left to run.
  uint64_t tickcount;  // Countdown to next timer interrupt.
  uint64_t interval;   // Initialize tickcount to this.

  unsigned cpu_id;

  CPUThread(const CPUThread &cputhread); // Don't allow copies
  void operator=(CPUThread &rval);       // or assignment.
};

// Paginator for long output
class Paginate {
public: 
  Paginate();
  void operator()(std::list<std::string> &lines);

private:
  int rows, cols; //Screen dimensions
  void pause();
};

extern Paginate paginate;

#endif
