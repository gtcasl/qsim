#include <stdio.h>
#include <stdint.h>

static inline void do_cpuid(uint32_t val) {
#if defined(__arm__) || defined(__aarch64__)
  asm volatile("msr pmcr_el0, %0" :: "r" (val));
#else
  asm("cpuid;\n":: "a"(val) : "%edx", "%ecx");
#endif
}

volatile int val = 0;

int main(int argc, char** argv)
{
  int i;
  do_cpuid(0xaaaaaaaa);

  for (i = 0; i < 10; i++)
    val += 1;

  do_cpuid(0xfa11dead);

  return 0;
}
