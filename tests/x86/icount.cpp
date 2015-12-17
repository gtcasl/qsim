#include<iostream>

#include "qsim_magic.h"

int main()
{
  qsim_magic_enable();
  asm volatile("mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n"
               "mfence;\n");
  qsim_magic_disable();

  return 0;
}
