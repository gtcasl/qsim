/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#ifndef __QSIM_LOCK_H
#define __QSIM_LOCK_H

#include <pthread.h>

#define QSIM_N_RWLOCKS 1024

typedef struct {
  pthread_rwlock_t  locks[QSIM_N_RWLOCKS];
} qsim_lockstruct;

static inline void qsim_lock_init(qsim_lockstruct *l) {
  unsigned i;
  for (i = 0; i < QSIM_N_RWLOCKS; ++i) {
    pthread_rwlock_init(&l->locks[i], NULL);
  }
}

static inline size_t qsim_lock_idx(uint64_t addr) {
  return (addr>>6)%QSIM_N_RWLOCKS;
}

static inline void qsim_lock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  pthread_rwlock_rdlock(&l->locks[idx]);
}

static inline void qsim_alock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  pthread_rwlock_wrlock(&l->locks[idx]);
}

static inline void qsim_unlock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  pthread_rwlock_unlock(&l->locks[idx]);
}

#endif
