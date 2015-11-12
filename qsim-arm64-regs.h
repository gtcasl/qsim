#ifndef __ARM64_REGS_H
#define __ARM64_REGS_H

enum _a64_regs {
    QSIM_X0 = 0, QSIM_X1, QSIM_X2, QSIM_X3,
    QSIM_X4, QSIM_X5, QSIM_X6, QSIM_X7,
    QSIM_X8, QSIM_X9, QSIM_X10, QSIM_X11,
    QSIM_X12, QSIM_X13, QSIM_X14, QSIM_X15,
    QSIM_X16, QSIM_X17, QSIM_X18, QSIM_X19,
    QSIM_X20, QSIM_X21, QSIM_X22, QSIM_X23,
    QSIM_X24, QSIM_X25, QSIM_X26, QSIM_X27,
    QSIM_X28, QSIM_X29, QSIM_X30,
    QSIM_CPSR64,
    QSIM_A64_N_REGS
};

static const char *a64_regs_str[] __attribute__((unused)) = {
    "x0", "x1", "x2", "x3",
    "x4", "x5", "x6", "x7",
    "x8", "x9", "x10", "x11",
    "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19",
    "x20", "x21", "x22", "x23",
    "x24", "x25", "x26", "x27",
    "x28", "x29", "x30",
    "cpsr64",
    NULL
};

uint64_t get_reg64(int r);
void     set_reg64(int r, uint64_t val );

#endif
