#include <qsim_magic.h>

int main(int argc, char* argv[]) {
  //int a = 0;
  qsim_magic_enable();
    /*if(argc == 2){
      a = 1;
    }
    if(argc == 3){
      a = 2;
    }
    if(argc == 4){
      a = 3;
    }
    if(argc == 5){
      a = 4;
    }*/
  asm volatile("cbz %0, 1f\n1:"::"r"(0));
  asm volatile("mov     w0, 1;\n"
               "str     w0, [sp, 28];\n"
               "ldr     w0, [sp, 12];\n"
               "cmp     w0, 2;\n"
               "bne     .L2;\n"
               "mov     w0, 2;\n"
               "str     w0, [sp, 28]\n"
               ".L2:\n"
               "ldr     w0, [sp, 12]\n"
               "cmp     w0, 3\n"
               "bne     .L3\n"
               "mov     w0, 3\n"
               "str     w0, [sp, 28]\n"
               ".L3:\n");
  qsim_magic_disable();

  return 0;
}





