#include <stdio.h>
#include <stdint.h>
#include <sys/sysinfo.h>

__attribute__((always_inline))
static inline void do_cpuid(uint32_t val) {
  asm("cpuid;\n":: "a"(val) : "ebx", "ecx", "edx");
}

int main(int argc, char** argv) {
  if      (argc == 1) do_cpuid(0xaaaaaaaa);
  else if (argc == 2) do_cpuid(0xfa11dead);

  // set number of processors in qsim
  asm volatile("cpuid;\n":: "a"(0xc5b1fffc), "b"(get_nprocs()):"ecx","edx");
  return 0;
}
