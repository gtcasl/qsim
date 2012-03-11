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

static inline void qsim_out(char i) {
  asm("cpuid;" : : "a" ((0xff&i)|0xc501e000) : "%ebx", "%edx", "%ecx");
}

static inline void qsim_bin_out(char i) {
  asm("cpuid;" : : "a" ((0xff&i)|0xc5b10000) : "%ebx", "%edx", "%ecx");
}

static inline char qsim_in() {
  char out;
  int ready;
  asm("cpuid;" : "=a"(ready) : "a"(0xc5b1fffe) : "%ebx", "%edx", "%ecx");
  if (!ready) exit(0);
  asm("cpuid;" : "=a"(out) : "a"(0xc5b1ffff) : "%ebx", "%edx", "%ecx");

  return out;
}

static inline size_t qsim_in_block(char* buf) {
  size_t s;
  asm("cpuid;" : "=c"(s) : "a"(0xc5b1fffd), "b"(buf) : "%edx");
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
