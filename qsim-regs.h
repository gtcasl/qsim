#ifndef __QSIM_REGS_H
#define __QSIM_REGS_H
/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/

#include "qsim-x86-regs.h"
#include "qsim-arm64-regs.h"
#include "qsim-arm-regs.h"

union regs {
  enum _x86_regs x86_regs;
  enum _a64_regs a64_regs;
  enum _arm_regs arm_regs;

  regs& operator=(const _x86_regs& rhs);
  regs& operator=(const _a64_regs& rhs);
  regs& operator=(const _arm_regs& rhs);
  regs(int i) { memset(this, i, sizeof(*this));}
};

#endif
