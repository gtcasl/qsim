#ifndef __i386_REGS_H
#define __i386_REGS_H

/* The ordering here is important: it is the order in which registers are saved
   and restored when we save or restore a machine state. */
enum regs {
  QSIM_RAX = 0, QSIM_RCX, QSIM_RDX, QSIM_RBX,
  QSIM_RSP, QSIM_RBP,
  QSIM_RSI, QSIM_RDI,
  QSIM_FP0, QSIM_FP1, QSIM_FP2, QSIM_FP3, QSIM_FP4, QSIM_FP5, QSIM_FP6, QSIM_FP7, QSIM_FPSP,
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

#endif
