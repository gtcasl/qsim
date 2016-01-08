#include <stdio.h>
#include <stdint.h>
#include <sys/sysinfo.h>

__attribute__((always_inline))
static inline void do_cpuid(uint32_t val) {
#if defined(__aarch64__)
  asm volatile("msr pmcr_el0, %0" :: "r" (val));
#else
  asm("cpuid;\n":: "a"(val) : "%ebx", "%ecx", "%edx");
#endif
}

static inline void set_n_cpus(void) {
#if defined(__aarch64__)
#else
  asm volatile("cpuid;\n":: "a"(0xc5b1fffc), "b"(get_nprocs()):"ecx","edx");
#endif
}

int main(int argc, char** argv) {
  if      (argc == 1) do_cpuid(0xaaaaaaaa);
  else if (argc == 2) do_cpuid(0xfa11dead);

  // set number of processors in qsim
  set_n_cpus();

  return 0;
}
