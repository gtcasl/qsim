#include <stdio.h>
#include <iostream>

#include "qsim_magic.h"

int main(int argc, char *argv[])
{
  char *p;
  p = new char[10];
  p[0] = 'a';
  qsim_magic_enable();
  asm volatile("ldr x1, [%0, #0]\n"
               "ldr x1, [%0, #1]\n"
               "ldr x1, [%0, #2]\n"
               "ldr x1, [%0, #3]\n"
               "ldr x1, [%0, #4]\n"
               "ldr x1, [%0, #5]\n"
               "ldr x1, [%0, #6]\n"
               "ldr x1, [%0, #7]\n"
               "ldr x1, [%0, #8]\n"
               "ldr x1, [%0, #9]\n"
               :: "r"(p));
  qsim_magic_disable();

  std::cout << "val is " << p[0] << std::endl;

  return 0;
}
