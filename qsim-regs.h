#ifndef __REGS_H
#define __REGS_H
/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/

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

/* The flags enum is used with the register access callback (size=0) to signal
   condition code access. */
enum flags {
  QSIM_FLAG_OF = 0x01,
  QSIM_FLAG_SF = 0x02,
  QSIM_FLAG_ZF = 0x04,
  QSIM_FLAG_AF = 0x08,
  QSIM_FLAG_PF = 0x10,
  QSIM_FLAG_CF = 0x20
};

/* Flags touched when the flags register is modified, and written by
   add/subtract instructions. */
#define QSIM_FLAG_ALL (QSIM_FLAG_OF|QSIM_FLAG_SF|QSIM_FLAG_ZF|QSIM_FLAG_AF|\
                       QSIM_FLAG_PF|QSIM_FLAG_CF)

/* Flags written by increment/decrement instructions: */
#define QSIM_FLAG_INC (QSIM_FLAG_OF|QSIM_FLAG_SF|QSIM_FLAG_ZF|QSIM_FLAG_AF|\
                       QSIM_FLAG_PF)

/* Flags written by bitwise logic instructions: */
#define QSIM_FLAG_LOG (QSIM_FLAG_OF|QSIM_FLAG_SF|QSIM_FLAG_ZF|QSIM_FLAG_PF|\
                       QSIM_FLAG_CF)

/* Flags written by rotate instructions: */
#define QSIM_FLAG_ROT (QSIM_FLAG_OF|QSIM_FLAG_CF)

#endif
