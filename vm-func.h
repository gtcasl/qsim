#ifndef __VM_FUNC__H
#define __VM_FUNC_H
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
#include "qsim-vm.h"

// Functions that QEMU must export
void qemu_init(qemu_ramdesc_t *ram, const char* ram_size, int cpu_id);

uint64_t run(uint64_t n);

int  interrupt(uint8_t vector);

void set_atomic_cb(atomic_cb_t);
void set_inst_cb  (inst_cb_t  );
void set_int_cb   (int_cb_t   );
void set_mem_cb   (mem_cb_t   );
void set_magic_cb (magic_cb_t );
void set_io_cb    (io_cb_t    );
void set_reg_cb   (reg_cb_t   );
void set_trans_cb (trans_cb_t );

uint64_t get_reg( enum regs r               );
void     set_reg( enum regs r, uint64_t val );

#ifdef __cplusplus
};
#endif
#endif
