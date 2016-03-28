#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#define ENABLE_QSIM 1

#if ENABLE_QSIM
#include <qsim_magic.h>
#else
#define qsim_magic_enable() do{}while(0)
#define qsim_magic_disable() do{}while(0)
#endif


volatile int value;
volatile int threads = 2;

void *thread1(void *arg)
{
  if (__sync_fetch_and_sub(&threads, 1) == 1)
    qsim_magic_enable();

  while(threads);

  for (uint64_t i = 0; i < 10000; i++)
    __sync_fetch_and_add(&value, i);

  return 0;
}

void *thread2(void *arg)
{
  if (__sync_fetch_and_sub(&threads, 1) == 1)
    qsim_magic_enable();

  while(threads);

  for (uint64_t i = 0; i < 10000; i++)
    __sync_fetch_and_add(&value, i);

  return 0;
}

int main()
{
  pthread_t t1, t2;

  printf("Creating threads...\n");
  pthread_create(&t1, NULL, thread1, NULL);
  pthread_create(&t2, NULL, thread2, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  printf("Killing threads...\n");

  qsim_magic_disable();

  printf("%d\n", value);
  return 0;
}
