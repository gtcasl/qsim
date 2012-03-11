/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define            N_THREADS             150
const unsigned int N_LOOPS_PER_THREAD  = 100000;

pthread_mutex_t    counter_mutex       = PTHREAD_MUTEX_INITIALIZER;
unsigned           counter             = 0;
pthread_barrier_t  all_spawned_barrier;
pthread_t          threads[N_THREADS];

void *thread_main(void *arg_vp) {
  volatile unsigned j = (unsigned)arg_vp;
  unsigned int i;

  pthread_mutex_lock(&counter_mutex); {
    counter++;
  } pthread_mutex_unlock(&counter_mutex);

  printf("test_threads: Now there are %u threads waiting\n", counter);

  pthread_barrier_wait(&all_spawned_barrier);

  for (i = 0; i < N_LOOPS_PER_THREAD; i++) {
    j ^= (i&(-i));
  }

  pthread_mutex_lock(&counter_mutex); {
    counter--;
  } pthread_mutex_unlock(&counter_mutex);

  return NULL;
}

void init_threads(void) {
  unsigned int i;

  pthread_barrier_init(&all_spawned_barrier, NULL, N_THREADS);

  for (i = 0; i < N_THREADS; i++) {
    int rval = pthread_create(&threads[i], NULL, thread_main, NULL);
    if (rval != 0) exit(1);
  }

  puts("test_threads: All threads spawned.\n");  
}

int main(void) {
  unsigned int i;

  init_threads();
  
  for (i = 0; i < N_THREADS; i++) pthread_join(threads[i], NULL);

  return 0;
}
