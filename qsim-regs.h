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

#define FOR_MACSIM 1

/* Possible values for "type" field of instruction callbacks. */
#if !FOR_MACSIM
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
#else
enum inst_type {
  QSIM_INST_INV,                      //!< invalid opcode
  QSIM_INST_SPEC,                     //!< something weird (rpcc)

  QSIM_INST_NOP,                      //!< is a decoded nop

  // these instructions use all integer regs
  QSIM_INST_CF,                       //!< change of flow
  QSIM_INST_CMOV,                     //!< conditional move
  QSIM_INST_LDA,                      //!< load address
  QSIM_INST_IMEM,                     //!< int memory instruction
  QSIM_INST_IADD,                     //!< integer add
  QSIM_INST_IMUL,                     //!< integer multiply
  QSIM_INST_ICMP,                     //!< integer compare
  QSIM_INST_LOGIC,                    //!< logical
  QSIM_INST_SHIFT,                    //!< shift
  QSIM_INST_BYTE,                     //!< byte manipulation
  QSIM_INST_MM,                       //!< multimedia instructions

  // fence instruction
  QSIM_INST_FENCE,
  QSIM_INST_ACQ_FENCE,
  QSIM_INST_REL_FENCE,

  // fmem reads one int reg and writes a fp reg
  QSIM_INST_FMEM,                     //!< fp memory instruction

  // everything below here is floating point regs only
  QSIM_INST_FCF,
  QSIM_INST_FCVT,                     //!< floating point convert
  QSIM_INST_FADD,                     //!< floating point add
  QSIM_INST_FMUL,                     //!< floating point multiply
  QSIM_INST_FDIV,                     //!< floating point divide
  QSIM_INST_FCMP,                     //!< floating point compare
  QSIM_INST_FBIT,                     //!< floating point bit
  QSIM_INST_FCMOV,                    //!< floating point cond move

  QSIM_INST_LD,                       //!< load memory instruction
  QSIM_INST_ST,                       //!< store memory instruction

  // MMX instructions
  QSIM_INST_SSE
};
#endif

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
