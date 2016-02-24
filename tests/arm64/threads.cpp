#include <stdio.h>
#include <qsim_magic.h>

#include <omp.h>

#define NUM_THREADS 5
#define NUM_ELEM 100000

int main()
{
	int p[NUM_ELEM], i;

	for (i = 0; i < NUM_ELEM; i++)
		p[i] = 0;

	omp_set_num_threads(NUM_THREADS);
	qsim_magic_enable();
	#pragma omp parallel for private(i) shared(p)
	for (i = 0; i < NUM_ELEM - 3; i++)
	{
		p[i]   += i;
		p[i+1] += (i+1);
		p[i+2] += (i+2);
		p[i+3] += (i+3);
	}
	qsim_magic_disable();

	return 0;
}
