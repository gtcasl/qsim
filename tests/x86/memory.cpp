#include<iostream>

#include "qsim_magic.h"

int main()
{
  qsim_magic_enable();
  asm volatile("mov $1, %eax\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n"
               "mov $1, %ebx\n");
  qsim_magic_disable();

  return 0;
}
