/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

static inline void do_cpuid(uint32_t val) {
#if defined(__aarch64__)
  asm volatile("msr pmcr_el0, %0" :: "r" (val));
#else
  asm("cpuid;\n":: "a"(val) : "%ebx", "%ecx", "%edx");
#endif
}

static inline int do_cpuid_return(uint32_t val) {
  int ret;

#if defined(__aarch64__)
  asm volatile("msr pmcr_el0, %0\n"
               "mov %0, x0\n"
               : "=r"(ret) : "r" (val));
#else
  asm("cpuid;\n": "=a"(ret) : "a"(val) : "%edx", "%ecx");
#endif

  return ret;
}

static inline int do_cpuid2_return(uint32_t val, char* buf) {
  int ret;

#if defined(__aarch64__)
  asm volatile("mov x0, %1\n"
               "mov x1, %2\n"
               "msr pmcr_el0, %0\n"
               "mov %0, x2\n"
               : "=&r"(ret) : "r" (val), "r" (buf));
#else
  asm("cpuid;\n": "=c"(ret) : "a"(val), "b"(buf) : "%edx");
#endif

  return ret;
}

static inline void qsim_out(char i) {
  do_cpuid((0xff & i) | 0xc501e000);
}

static inline void qsim_bin_out(char i) {
  do_cpuid((0xff & i) | 0xc5b10000);
}

static inline char qsim_in() {
  char out;
  int ready;
  ready = do_cpuid_return(0xc5b1fffe);
  if (!ready) exit(0);
  out = do_cpuid_return(0xc5b1ffff);

  return out;
}

static inline size_t qsim_in_block(char* buf) {
  size_t s;
  s = do_cpuid2_return(0xc5b1fffd, buf);

  return s;
}

int main(int argc, char **argv) {
  int c;
  if (!strcmp(argv[0], "/sbin/qsim_out")) {
    while ((c = fgetc(stdin)) != EOF ) qsim_out(c);
  } else if (!strcmp(argv[0], "/sbin/qsim_bin_out")) {
    while ((c = fgetc(stdin)) != EOF) qsim_bin_out(c);
  } else if (!strcmp(argv[0], "/sbin/qsim_in")) {
    //do { char c = qsim_in(); write(1, &c, 1); } while(1);
    size_t s;
    char buf[1024];
    while ((s = qsim_in_block(buf)) != 0) write(1, buf, s);
  }

  return 0;
}
