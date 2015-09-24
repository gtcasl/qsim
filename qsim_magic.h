#ifndef _QSIM_MAGIC_H
#define _QSIM_MAGIC_H

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

#endif /* _QSIM_MAGIC_H */
