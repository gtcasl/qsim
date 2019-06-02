#include <stdio.h>
#include "qsim_magic.h"
#include <stdint.h>

uint64_t arr[10];

void print_cpsr()
{

	int i = 0;
	qsim_magic_enable();
	for (i = 0; i < 10; i++)
		arr[i] = 0xffff1234 & i;

	for (i = 0; i < 10; i++)
		arr[i] *= i;
	qsim_magic_disable();

	return;
}

int main()
{
	print_cpsr();
	return 0;
}
