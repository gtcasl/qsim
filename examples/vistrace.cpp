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
#include <Imlib2.h>

#include <qsim.h>

using std::cout; using std::vector; using std::ofstream; using std::string;
using Qsim::OSDomain; using std::map;

const uint64_t OFFSET        = (3072l<<20) + (4096l<<12);
const uint64_t GRAN          = 4l<<10       ;
const uint64_t RANGE         = 32l<<20      ;
const unsigned ROWS_PER_MILN = 100          ;
const unsigned VERT_DOWNSAMP = 1            ;
const unsigned MILLION_INSTS = 400          ;
const unsigned N_CPUS        = 4            ;
const uint32_t COLOR_GRAY    = 0x000f0f0f   ;
const uint32_t ONE_RED       = 0x00010000   ;
const uint32_t COLOR_RED     = 0x00ff0000   ;
const uint32_t ONE_GREEN     = 0x00000100   ;
const uint32_t COLOR_GREEN   = 0x0000ff00   ;
const uint32_t ONE_BLUE      = 0x00000001   ;
const uint32_t COLOR_BLUE    = 0x000000ff   ;

double         current_max_red   = 0;
double         current_max_green = 0;
double         current_max_blue  = 0;
vector<double> current_row_red  (RANGE/GRAN);
vector<double> current_row_green(RANGE/GRAN);
vector<double> current_row_blue (RANGE/GRAN);

map <uint16_t, uint64_t> memop_counts;
map <uint16_t, uint64_t> kernel_inst_counts;
map <uint16_t, uint64_t> user_inst_counts;
map <uint16_t, uint64_t> pagefault_counts;
map <uint16_t, uint64_t> cpu_inst_counts;

pthread_barrier_t cpu_barrier1;
pthread_barrier_t cpu_barrier2;

unsigned          img_row;
Imlib_Image       img_buf_iml;
DATA32*           img_buf;

pthread_mutex_t app_end_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t app_end = PTHREAD_COND_INITIALIZER;
bool app_started = false;

struct thread_arg_t {
  int       cpu   ;
  OSDomain  *cd    ;
  bool      atomic;
  bool      draw_atomic;
  uint64_t  last_vaddr;
};

vector<thread_arg_t*> thread_args(N_CPUS);
vector<pthread_t   *> threads    (N_CPUS);

void next_row() {
  // First, create the normalized pixel data.
  for (unsigned i = 0; i < RANGE/GRAN; i++) { 
    img_buf[RANGE/GRAN*img_row + i] |= 
      ((uint8_t)((current_row_red[i]/*>0*//current_max_red)*255)) << 16;
    current_row_red[i] = 0;
  }
  current_max_red = 0;

  for (unsigned i = 0; i < RANGE/GRAN; i++) {
    img_buf[RANGE/GRAN*img_row + i] |=
      ((uint8_t)((current_row_green[i]/*>0*//current_max_green)*255)) << 8;
    current_row_green[i] = 0;
  }
  current_max_green = 0;

  for (unsigned i = 0; i < RANGE/GRAN; i++) {
    img_buf[RANGE/GRAN*img_row + i] |=
      ((uint8_t)((current_row_blue[i]/*>0*//current_max_blue)*255));
    current_row_blue[i] = 0;
  }
  current_max_blue = 0;  

  // Next, fill in the gray "shadow" pixels.
#if 0
  if (img_row > 0) {
    for (unsigned i = 0; i < RANGE/GRAN; i++) {
      if (img_buf[RANGE/GRAN*(img_row-1) + i] && 
	  !img_buf[RANGE/GRAN*img_row +i])
	img_buf[RANGE/GRAN*img_row + i] = COLOR_GRAY;
    }
  }
#endif

  // Finally, increment the row index.
  if (app_started) img_row++;
}

void *cpu_thread_main(void* thread_arg) {
  thread_arg_t *arg = (thread_arg_t*)thread_arg;
  // The first barrier. Wait for all of the threads to get initialized.
  pthread_barrier_wait(&cpu_barrier1);

  if (arg->cpu == 0) cout << "QTM threads ready.\n";

  // Outer loop: run for MILLION_INSTS million instructions
  for (unsigned i = 0; i < MILLION_INSTS * ROWS_PER_MILN; i++) {
    unsigned countdown = 1000000/ROWS_PER_MILN;
    while (countdown > 0) {
      while (arg->atomic && countdown > 0) {
        arg->atomic = false;
	arg->draw_atomic = true;
	countdown -= arg->cd->run(arg->cpu, 1);
	arg->draw_atomic = false;
      }

      countdown -= arg->cd->run(arg->cpu, countdown);

      // This CPU may not be running yet. Get out and wait for the barrier.
      if (countdown == 1000000/ROWS_PER_MILN && !arg->atomic) break;
    }
    // We call the timer interrupt in one thread, while no others are running.
    pthread_barrier_wait(&cpu_barrier1);

    static unsigned last_tick_announcement = 0;
    if (arg->cpu == 0) {
      if (i % ROWS_PER_MILN == (ROWS_PER_MILN - 1)) {
        std::cout << "Timer interrupt.\n";
        arg->cd->timer_interrupt();
	if (img_row*VERT_DOWNSAMP/ROWS_PER_MILN % 200 == 0 
            && last_tick_announcement != img_row) {
	  cout << "Tick " << img_row * VERT_DOWNSAMP / ROWS_PER_MILN << '\n';
	  last_tick_announcement = img_row;
	}
      }
      if ( i % VERT_DOWNSAMP == (VERT_DOWNSAMP - 1)) next_row();
    }
    pthread_barrier_wait(&cpu_barrier2);
  }

  return NULL;
}

int atomic_cb(int cpu_id) {
  // Set the atomic flag for this CPU and tell it to exit before executing the
  // atomic instruction.
  thread_args[cpu_id]->atomic = true;
 
  return 1;
}

void mark_red(uint64_t paddr) {
  if ((paddr-OFFSET) > RANGE) return;
  double &pix = current_row_red[(paddr-OFFSET)/GRAN];
  pix++;
  if (pix > current_max_red) current_max_red = pix;
}

void mark_grn(uint64_t paddr) {
  if ((paddr-OFFSET) > RANGE) return;
  double &pix = current_row_green[(paddr-OFFSET)/GRAN];
  pix++;
  if (pix > current_max_green) current_max_green = pix;
}

void mark_blu(uint64_t paddr) {
  if ((paddr-OFFSET) > RANGE) return;
  double &pix = current_row_blue[(paddr-OFFSET)/GRAN];
  pix++;
  if (pix > current_max_blue) current_max_blue = pix;
}

void inst_cb(int            cpu_id, 
	     uint64_t       vaddr,
	     uint64_t       paddr, 
	     uint8_t        len, 
	     const uint8_t *bytes,
             enum inst_type type) 
{
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);

  cpu_inst_counts[cpu_id]++;

  if (thread_args[cpu_id]->cd->get_prot(cpu_id) == OSDomain::PROT_KERN) {
    kernel_inst_counts[tid]++;
  } else {
    user_inst_counts[tid]++;
  }

  if (len == 1 && *bytes == 0xf4) {
    thread_args[cpu_id]->cd->timer_interrupt();
  }

  mark_red(vaddr);

  // color-coded CPU fun
  //if (cpu_id == 0) mark_red(paddr);
  //else if (cpu_id == 1) mark_blu(paddr);
  //else mark_grn(paddr);
}

int int_cb(int cpu_id, uint8_t vec) 
{
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);
  //cout << "int 0x" << std::hex << (unsigned)vec << " CPU " << cpu_id << '\n';
  if (vec == 0x0e) {
    pagefault_counts[tid]++;
  }
  return 0;
}

void mem_cb(int cpu_id, uint64_t vaddr, uint64_t paddr, uint8_t size, int type)
{
  uint16_t tid = thread_args[cpu_id]->cd->get_tid(cpu_id);
  memop_counts[tid]++;
  thread_args[cpu_id]->last_vaddr = vaddr;
  //if (type) { 
  //  if (thread_args[cpu_id]->draw_atomic) mark_grn (paddr);
  //  else                                  mark_blu (paddr);
  //}

  if (type) mark_blu(vaddr); else mark_grn(vaddr);

  //color-coded CPU fun
  //if (cpu_id == 0) mark_red(paddr);
  //else if (cpu_id == 1) mark_grn(paddr);
  //else mark_blu(paddr);
}

void app_start_cb(int cpu_id) {
  next_row();
  for (unsigned int i = 0; i != RANGE/GRAN; i++) {
    current_row_red[i] = current_row_green[i] = current_row_blue[i] = 1.0;
  }
  current_max_red = current_max_green = current_max_blue = 1.0;
  app_started = true;
  next_row();
}

void app_end_cb(int cpu_id) {
  pthread_mutex_lock(&app_end_lock);
  pthread_cond_signal(&app_end);
  pthread_mutex_unlock(&app_end_lock);
}

int main(int argc, char** argv) {
  // Create output image buffer.
  unsigned VERT_RES = MILLION_INSTS * ROWS_PER_MILN / VERT_DOWNSAMP + 2;
  img_buf_iml = imlib_create_image(RANGE/GRAN, VERT_RES);
  img_row = 0;
  imlib_context_set_image   (img_buf_iml              );
  imlib_context_set_color   (0, 0, 0,    255          );
  imlib_image_fill_rectangle(0, 0, RANGE/GRAN, VERT_RES);
  img_buf = imlib_image_get_data();

  // Init. sync objects
  pthread_barrier_init(&cpu_barrier1, NULL, N_CPUS);
  pthread_barrier_init(&cpu_barrier2, NULL, N_CPUS);

  // Create a runnable OSDomain.
  OSDomain *cd = new OSDomain(N_CPUS, "linux/bzImage", 1024);

  // Set callbacks.
  cd->set_atomic_cb  (atomic_cb);
  cd->set_inst_cb    (inst_cb  );
  cd->set_int_cb     (int_cb   );
  cd->set_mem_cb     (mem_cb   );
  cd->connect_console(cout     );
  //cd->set_app_start_cb(app_start_cb);
  app_start_cb(0);
  cd->set_app_end_cb(app_end_cb);

  // Launch threads
  for (unsigned i = 0; i < N_CPUS; i++) {
    threads[i]     = new pthread_t    ;
    thread_args[i] = new thread_arg_t ;

    thread_args[i]->cpu    = i;
    thread_args[i]->cd     = cd;
    thread_args[i]->atomic = false;

    pthread_create(threads[i], NULL, cpu_thread_main, thread_args[i]);
  }

  // Wait for threads to return
  for (unsigned i = 0; i < N_CPUS; ++i) pthread_join(*threads[i], NULL);
  //pthread_mutex_lock(&app_end_lock);
  //pthread_cond_wait(&app_end, &app_end_lock);

  // Print stats
  for( map<uint16_t, uint64_t>::iterator i = memop_counts.begin();
       i != memop_counts.end();
       i++) 
  {
    cout << "TID " << i->first << ": " << std::dec << i->second 
         << " mem ops, " << user_inst_counts[i->first] << '/' 
         << kernel_inst_counts[i->first] << "(usr/krnl) insts, "
         << pagefault_counts[i->first] << " page faults\n";
  }

  for (int i = 0; i < N_CPUS; i++) {
    cout << "CPU " << i << ": " << cpu_inst_counts[i] << " instructions.\n"; 
  }

  // Clean up
#if 0
  pthread_barrier_destroy(&cpu_barrier1);
  pthread_barrier_destroy(&cpu_barrier2);
  for (unsigned i = 0; i < N_CPUS; i++) { 
    delete threads[i]; 
    delete thread_args[i];
  }
#endif

  imlib_image_put_back_data(img_buf             );
  imlib_save_image         ("vtr_access_map.png");

  return 0;
}
