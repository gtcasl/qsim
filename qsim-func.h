#ifndef __VM_FUNC_H
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
#include <stdbool.h>
#include "qsim-vm.h"

// Functions that QEMU must export
void qemu_init(const char* argv[]);

uint64_t run(uint64_t n);
uint64_t run_cpu(int i, uint64_t n);

void set_atomic_cb(atomic_cb_t);
void set_inst_cb  (inst_cb_t  );
void set_brinst_cb(brinst_cb_t);
void set_int_cb   (int_cb_t   );
void set_mem_cb   (mem_cb_t   );
void set_magic_cb (magic_cb_t );
void set_io_cb    (io_cb_t    );
void set_reg_cb   (reg_cb_t   );
void set_trans_cb (trans_cb_t );
void set_gen_cbs  (bool state );
void set_sys_cbs  (bool state );

#ifdef __cplusplus
};
#endif
#endif
