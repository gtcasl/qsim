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

#ifdef QSIM_USE_PTHREAD_RWLOCK
#include <pthread.h>
#include <stdint.h>
typedef pthread_rwlock_t qsim_rwlock_t;
#define qsim_rwlock_init(l) do { pthread_rwlock_init((l), NULL); } while (0)
#define qsim_rwlock_rdlock(l) do { pthread_rwlock_rdlock(l); } while (0)
#define qsim_rwlock_wrlock(l) do { pthread_rwlock_wrlock(l); } while (0)
#define qsim_rwlock_wrunlock(l) do { pthread_rwlock_unlock(l); } while (0)
#define qsim_rwlock_rdunlock(l) do { pthread_rwlock_unlock(l); } while (0)
#else
#include "qsim-rwlock.h"
#endif

#define QSIM_N_RWLOCKS 0x10000
#define QSIM_BLOCK_SZ 64

typedef struct {
  qsim_rwlock_t  locks[QSIM_N_RWLOCKS];
} qsim_lockstruct __attribute__ ((aligned(64)));

static inline void qsim_lock_init(qsim_lockstruct *l) {
  unsigned i;
  for (i = 0; i < QSIM_N_RWLOCKS; ++i) qsim_rwlock_init(&l->locks[i]);
}

static inline size_t qsim_lock_idx(uint64_t addr) {
  return ((addr/QSIM_BLOCK_SZ*QSIM_BLOCK_SZ) & 0xffff)
          ^ ((addr>>16) & 0xffff)
          ^ (((addr>>32) & 0xffff) ^ ((addr>>40) & 0xffff));
}

static inline void qsim_lock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  qsim_rwlock_rdlock(&l->locks[idx]);
}

static inline void qsim_alock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  qsim_rwlock_wrlock(&l->locks[idx]);
}

static inline void qsim_unlock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  qsim_rwlock_rdunlock(&l->locks[idx]);
}

static inline void qsim_aunlock_addr(qsim_lockstruct *l, uint64_t addr) {
  size_t idx = qsim_lock_idx(addr);
  qsim_rwlock_wrunlock(&l->locks[idx]);
}

#endif
