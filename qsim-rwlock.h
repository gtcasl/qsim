/*****************************************************************************
 * This is a readers/writer spinlock similar to the one in the Linux kernel. *
 *****************************************************************************/
#ifndef __QSIM_RWLOCK_H
#define __QSIM_RWLOCK_H

#define QSIM_RWLOCK_BIAS 0x10000

#define QSIM_PADDED_LOCKS

typedef struct qsim_rwlock {
  int i;
#ifdef QSIM_PADDED_LOCKS
  int _padding[15]; /* With padding, there is one lock per cache line. */
#endif
} qsim_rwlock_t;

static inline void qsim_rwlock_init(qsim_rwlock_t *l) {
  l->i = QSIM_RWLOCK_BIAS;
}

static inline void qsim_rwlock_rdlock(qsim_rwlock_t *l) {
  __asm__ __volatile__ ("1: lock subl $1,(%0);\n"
                        "jns 3f;\n"
                        "lock addl $1,(%0);\n"
                        "2: pause;\n"
                        "cmpl $1, (%0);\n"
                        "js 2b;\n"
                        "jmp 1b;\n"
                        "3:\n":: "a"(&l->i): "memory");
}

static inline void qsim_rwlock_wrlock(qsim_rwlock_t *l) {
  __asm__ __volatile__ ("1: lock subl %1,(%0);\n"
                        "jz 3f;\n"
                        "lock addl %1, (%0);\n"
                        "2: pause;\n"
                        "cmpl %1, (%0);\n"
                        "jnz 2b;\n"
                        "jmp 1b;\n"
                        "3:\n"::
                        "a"(&l->i), "i"(QSIM_RWLOCK_BIAS): "memory");
}

static inline void qsim_rwlock_rdunlock(qsim_rwlock_t *l) {
  __asm__ __volatile__ ("lock addl $1, (%0);\n":: "a"(&l->i): "memory");

}

static inline void qsim_rwlock_wrunlock(qsim_rwlock_t *l) {
  __asm__ __volatile__ ("lock addl %1,(%0);\n"::
                        "a"(&l->i), "i"(QSIM_RWLOCK_BIAS): "memory");
}

#endif
