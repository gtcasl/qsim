#ifndef _QSIM_MAGIC_H
#define _QSIM_MAGIC_H

#include <signal.h>
#include <stdlib.h>

#define qsim_magic_enable()				\
do {							\
	asm volatile (  "1:mrs %0, DCZID_EL0\n\t"	\
			"mrs %0, DCZID_EL0\n\t"		\
			"mrs %0, DCZID_EL0\n\t"		\
			"mrs %0, DCZID_EL0\n\t"		\
			"mrs %0, DCZID_EL0\n\t"		\
			"cbz %0, 1b\n\t"		\
			:: "r" (0));			\
} while(0);

#define qsim_magic_disable() qsim_magic_enable()

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
