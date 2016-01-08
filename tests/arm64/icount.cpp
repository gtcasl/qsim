#include<iostream>

#include "qsim_magic.h"

int main()
{
  qsim_magic_enable();
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
