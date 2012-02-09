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

/* The ordering here is important: it is the order in which registers are saved
   and restored when we save or restore a machine state. */
enum regs {
  QSIM_RAX = 0, QSIM_RCX, QSIM_RDX, QSIM_RBX,
  QSIM_RSP, QSIM_RBP,
  QSIM_RSI, QSIM_RDI,
  QSIM_CR0, QSIM_CR2, QSIM_CR3, QSIM_CR4,
  QSIM_GDTB, QSIM_IDTB, QSIM_GDTL, QSIM_IDTL,
  QSIM_TR, QSIM_TRB, QSIM_TRL, QSIM_TRF,
  QSIM_LDT, QSIM_LDTB, QSIM_LDTL, QSIM_LDTF,
  QSIM_DR6, QSIM_DR7,
  QSIM_ES,  QSIM_CS,  QSIM_SS,  QSIM_DS,  QSIM_FS,  QSIM_GS,
  QSIM_ESB, QSIM_CSB, QSIM_SSB, QSIM_DSB, QSIM_FSB, QSIM_GSB,
  QSIM_ESL, QSIM_CSL, QSIM_SSL, QSIM_DSL, QSIM_FSL, QSIM_GSL,
  QSIM_ESF, QSIM_CSF, QSIM_SSF, QSIM_DSF, QSIM_FSF, QSIM_GSF,
  QSIM_RIP, QSIM_RFLAGS,
  QSIM_HFLAGS, QSIM_HFLAGS2,
  QSIM_SE_CS, QSIM_SE_SP, QSIM_SE_IP,
  QSIM_N_REGS
};

/* This has to be manually kept consistent with the above. Ugly, I know. */
static const char *regs_str[] = {
  "rax", "rcx", "rdx", "rbx",
  "rsp", "rbp",
  "rsi", "rdi",
  "cr0", "cr2", "cr3", "cr4",
  "gdtb", "idtb", "gdtl", "idtl",
  "tr", "trb", "trl", "trf",
  "ldt", "ldtb", "ldtl", "ldtf",
  "dr6", "dr7",
  "es", "cs", "ss", "ds", "fs", "gs",
  "esb", "csb", "ssb", "dsb", "fsb", "gsb",
  "esl", "csl", "ssl", "dsl", "fsl", "gsl",
  "esf", "csf", "ssf", "dsf", "fsf", "gsf",
  "rip", "rflags",
  "hflags", "hflags2",
  "se_cs", "se_sp", "se_ip",
  NULL
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
