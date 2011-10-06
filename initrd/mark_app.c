#include <stdio.h>
#include <stdint.h>

static inline void do_cpuid(uint32_t val) {
  asm("cpuid;\n":: "a"(val) : "%edx", "%ecx");
}

int main(int argc, char** argv) {
  if      (argc == 1) do_cpuid(0xaaaaaaaa);
  else if (argc == 2) do_cpuid(0xfa11dead);
  return 0;
}
