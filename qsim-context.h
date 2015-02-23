#ifndef QSIM_CONTEXT_H
#define QSIM_CONTEXT_H

#if 0
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
typedef struct {
  struct {
    size_t ss_size;
    void  *ss_base;
    void  *ss_sp;
  } uc_stack;
  void *uc_link;
} qsim_ucontext_t;

static void swapcontext(qsim_ucontext_t *from, qsim_ucontext_t *to)
  __attribute__((noinline));
static void makecontext(qsim_ucontext_t *p, void(*f)(void), unsigned args)
  __attribute__((noinline));
static void getcontext(qsim_ucontext_t *p);

static __attribute__((unused)) void swapcontext(qsim_ucontext_t *from, qsim_ucontext_t *to) {
#ifdef __i386__
  asm("push %%ebp;"
      "push %%ebx;"
      "push %%esi;"
      "push %%edi;"
      "sub $6, %%esp;"
      "fstcw 4(%%esp);"
      "stmxcsr (%%esp);"
      "mov %%esp, (%%eax);"
      "mov (%%ecx), %%esp;"
      "ldmxcsr (%%esp);"
      "fldcw 4(%%esp);"
      "add $6, %%esp;"
      "pop %%edi;"
      "pop %%esi;"
      "pop %%ebx;"
      "pop %%ebp;"
      "leave;"
      "ret;" :: "a"(&(from->uc_stack.ss_sp)), "c"(&(to->uc_stack.ss_sp)));
#elif __x86_64__
  asm("push %%rdi;"        // Save 'from' registers
      "push %%rbp;"
      "push %%rbx;"
      "push %%r12;"
      "push %%r13;"
      "push %%r14;"
      "push %%r15;"
      "sub $6, %%rsp;"
      "fstcw 4(%%rsp);"
      "stmxcsr (%%rsp);"
      "mov %%rsp, (%%rdi);"// Save old stack pointer
      "mov (%%rsi), %%rsp;"// Load new stack pointer
      "ldmxcsr (%%rsp);"
      "fldcw 4(%%rsp);"
      "add $6, %%rsp;"
      "pop %%r15;"         // Restore 'to' registers
      "pop %%r14;"
      "pop %%r13;"
      "pop %%r12;"
      "pop %%rbx;"
      "pop %%rbp;"
      "pop %%rdi;"
#ifndef __OPTIMIZE__
      "leave;"
#endif
      "ret;" :: "D"(&(from->uc_stack.ss_sp)), "S"(&(to->uc_stack.ss_sp)));
#endif
}

static __attribute__((unused)) void makecontext(qsim_ucontext_t *p, void (*f)(void), unsigned args) {
  // Set initial stack pointer. The stack grows down, so the size is needed.
  p->uc_stack.ss_base = p->uc_stack.ss_sp;
  p->uc_stack.ss_sp += (size_t)p->uc_stack.ss_size - sizeof(void*)*9;

  void **stack = (void**)p->uc_stack.ss_sp;

  unsigned i;
  for (i = 2; i < 9; i++) stack[i] = (void*)(0x57ac0l + i);
#ifdef __i386__
  stack[4] = (void*)f;
  stack[5] = (void*)f;
  stack[3] = (void*)((char*)p->uc_stack.ss_sp + sizeof(void*)*4);
  p->uc_stack.ss_sp = (uint8_t*)stack - 6;
  asm("stmxcsr (%%eax); fstcw 4(%%eax);" :: "a"(p->uc_stack.ss_sp));
#elif __x86_64__
  stack[7] = (void*)f;           // One of these, depending on whether the
  stack[8] = (void*)f;           // base pointer is saved.
  stack[5] = (void*)((char*)p->uc_stack.ss_sp + sizeof(void*)*7); // Init. bp
  p->uc_stack.ss_sp = (uint8_t*)stack - 6;
  asm("stmxcsr (%%rax); fstcw 4(%%rax);" :: "a"(p->uc_stack.ss_sp));
#endif
}

static __attribute__((unused)) void getcontext(qsim_ucontext_t *p) {}

#endif
#endif
#include <ucontext.h>
typedef ucontext_t qsim_ucontext_t;

#endif

