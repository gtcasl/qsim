#include <pthread.h>

#include "qcache.h"

pthread_mutex_t Qcache::errLock = PTHREAD_MUTEX_INITIALIZER;
bool Qcache::printResults = false;
