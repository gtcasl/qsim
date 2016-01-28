#ifndef __VM_H
#define __VM_H
/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <pthread.h>

#include "qsim-lock.h"

/* Possible values for "type" field of instruction callbacks. */
enum inst_type {
  QSIM_INST_NULL,     /* NOP or load/store only. */
  QSIM_INST_INTBASIC, /* Simple ALU operation. */
  QSIM_INST_INTMUL,
  QSIM_INST_INTDIV,
  QSIM_INST_STACK,
  QSIM_INST_BR,       /* Branch. */
  QSIM_INST_CALL,
  QSIM_INST_RET,
  QSIM_INST_TRAP,     /* Interrupt/syscall. */
  QSIM_INST_FPBASIC,  /* Floating point add, subtract */
  QSIM_INST_FPMUL,
  QSIM_INST_FPDIV
};

enum qsim_mode {
  QSIM_HEADLESS,
  QSIM_INTERACTIVE,
  QSIM_KVM,
  QSIM_NUM_MODES
};

typedef void (*inst_cb_t)(int cpu_id, uint64_t vaddr, uint64_t paddr,
                          uint8_t length, const uint8_t *bytes,
                          enum inst_type type);
typedef void (*mem_cb_t)(int cpu_id, uint64_t vaddr, uint64_t paddr,
                        uint8_t  size, int type);
typedef uint32_t* (*io_cb_t) (int cpu_id, uint64_t addr, uint8_t size,
                              int type, uint32_t val);

typedef int (*int_cb_t)(int cpu_id, uint8_t  vector);
typedef int (*magic_cb_t) (int cpu_id, uint64_t rax);
typedef int (*atomic_cb_t)(int cpu_id);
typedef void (*reg_cb_t)(int cpu_id, int reg, uint8_t  val, int type);
typedef void (*trans_cb_t)(int cpu_id);

typedef struct {
  uint8_t *mem_ptr;
  size_t   sz;
  qsim_lockstruct *l;
} qemu_ramdesc_t;


#ifdef __cplusplus
};
#endif
#endif
