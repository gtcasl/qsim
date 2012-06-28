#ifndef __QCACHE_MOESI_H
#define __QCACHE_MOESI_H

// There is a MUCH MUCH nicer way to do this in C++ 11 with alias templates, but
// this was decided to be the most elegant way to do this without violating
// earlier, more widely supported C++ standards
#define __QCACHE_DEF_MOESI 1
#include "qcache-mesi.h"
#undef __QCACHE_DEF_MOESI

#endif
