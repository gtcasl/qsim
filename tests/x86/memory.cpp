#include <iostream>

#include "qsim_magic.h"

int main(int argc, char *argv[])
{
  int *p;
  p = new int[argc];
  qsim_magic_enable();
  asm volatile("cmp %eax, %eax;\nje label\nlabel:");
  asm volatile("movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               "movl $1, (%1)\n"
               :"+r"(p));
  qsim_magic_disable();

  std::cout << "val is " << p[0] << std::endl;

  return 0;
}
