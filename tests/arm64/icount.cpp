#include<iostream>

#include "qsim_magic.h"

int main()
{
  qsim_magic_enable();
  asm volatile("cbz %0, 1f\n1:"::"r"(0));
  asm volatile("dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n"
               "dmb ishst;\n");
  qsim_magic_disable();

  return 0;
}
