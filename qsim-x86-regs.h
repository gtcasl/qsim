#ifndef __x86_REGS_H
#define __x86_REGS_H
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

/* The ordering here is important: it is the order in which registers are saved
   and restored when we save or restore a machine state. */
enum qsim_x86_reg {
  QSIM_X86_RAX = 0, QSIM_X86_RCX, QSIM_X86_RDX, QSIM_X86_RBX,
  QSIM_X86_RSP, QSIM_X86_RBP,
  QSIM_X86_RSI, QSIM_X86_RDI,
  QSIM_X86_R7, QSIM_X86_R8, QSIM_X86_R9, QSIM_X86_R10,
  QSIM_X86_R11, QSIM_X86_R12, QSIM_X86_R13, QSIM_X86_R14, QSIM_X86_R15,
  QSIM_X86_FP0, QSIM_X86_FP1, QSIM_X86_FP2, QSIM_X86_FP3, QSIM_X86_FP4, QSIM_X86_FP5, QSIM_X86_FP6, QSIM_X86_FP7, QSIM_X86_FPSP,
  QSIM_X86_CR0, QSIM_X86_CR2, QSIM_X86_CR3, QSIM_X86_CR4,
  QSIM_X86_GDTB, QSIM_X86_IDTB, QSIM_X86_GDTL, QSIM_X86_IDTL,
  QSIM_X86_TR, QSIM_X86_TRB, QSIM_X86_TRL, QSIM_X86_TRF,
  QSIM_X86_LDT, QSIM_X86_LDTB, QSIM_X86_LDTL, QSIM_X86_LDTF,
  QSIM_X86_DR6, QSIM_X86_DR7,
  QSIM_X86_ES,  QSIM_X86_CS,  QSIM_X86_SS,  QSIM_X86_DS,  QSIM_X86_FS,  QSIM_X86_GS,
  QSIM_X86_ESB, QSIM_X86_CSB, QSIM_X86_SSB, QSIM_X86_DSB, QSIM_X86_FSB, QSIM_X86_GSB,
  QSIM_X86_ESL, QSIM_X86_CSL, QSIM_X86_SSL, QSIM_X86_DSL, QSIM_X86_FSL, QSIM_X86_GSL,
  QSIM_X86_ESF, QSIM_X86_CSF, QSIM_X86_SSF, QSIM_X86_DSF, QSIM_X86_FSF, QSIM_X86_GSF,
  QSIM_X86_RIP, QSIM_X86_RFLAGS,
  QSIM_X86_HFLAGS, QSIM_X86_HFLAGS2,
  QSIM_X86_SE_CS, QSIM_X86_SE_SP, QSIM_X86_SE_IP,
  QSIM_X86_N_REGS
};

/* This has to be manually kept consistent with the above. Ugly, I know. */
static const char *x86_regs_str[] __attribute__((unused)) = {
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

#ifdef __cplusplus
}
#endif

#endif /* __x86_REGS_H */
