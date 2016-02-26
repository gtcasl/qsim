#include<iostream>

#include "qsim_magic.h"

int main()
{
  qsim_magic_enable();
  asm volatile("cmp %eax, %eax;\nje label\nlabel:");
  asm volatile("movl $1, %eax;\n");
  asm volatile("movl $1, %ebx;\n");
  asm volatile("movl $1, %ecx;\n");
  asm volatile("movl $1, %edx;\n");
  asm volatile("mov $1, %r8;\n");
  asm volatile("mov $1, %r9;\n");
  asm volatile("mov $1, %r10;\n");
  asm volatile("mov $1, %r11;\n");
  asm volatile("mov $1, %r12;\n");
  asm volatile("mov $1, %r13;\n");
  asm volatile("mov $1, %r14;\n");
  asm volatile("mov $1, %r15;\n");
  asm volatile("addl $1, %eax;\n");
  asm volatile("addl $1, %ebx;\n");
  asm volatile("addl $1, %ecx;\n");
  asm volatile("addl $1, %edx;\n");
  asm volatile("add $1, %r8;\n");
  asm volatile("add $1, %r9;\n");
  asm volatile("add $1, %r10;\n");
  asm volatile("add $1, %r11;\n");
  asm volatile("add $1, %r12;\n");
  asm volatile("add $1, %r13;\n");
  asm volatile("add $1, %r14;\n");
  asm volatile("add $1, %r15;\n");
  qsim_magic_disable();

  return 0;
}
