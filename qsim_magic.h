#ifndef _QSIM_MAGIC_H
#define _QSIM_MAGIC_H

#include <signal.h>
#include <stdlib.h>

#define qsim_magic_enable()				\
	asm volatile("msr pmcr_el0, %0" :: "r" (0xaaaaaaaa));
#define qsim_magic_disable() 				\
	asm volatile("msr pmcr_el0, %0" :: "r" (0xfa11dead));

static void qsim_sig_handler(int signo)
{
	char c;
	qsim_magic_disable();
	printf("Callbacks disabled. Quit(y/n)?");
	scanf("%c", &c);

	if (c == 'y')
		exit(0);
}

#define qsim_init()					\
do {							\
	signal(SIGINT, qsim_sig_handler);		\
} while(0);

#endif /* _QSIM_MAGIC_H */
