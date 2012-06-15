#ifndef __QCACHE_MSI_H
#define __QCACHE_MSI_H

// There is a MUCH MUCH nicer way to do this in C++ 11 with alias templates, but
// this was decided to be the most elegant way to do this without violating
// earlier, more widely supported C++ standards
#define __QCACHE_DEF_MSI 1
#include "qcache-mesi.h"
#undef __QCACHE_DEF_MSI

#endif
