#ifndef __DEBUG_H
#define __DEBUG_H

#include <iostream>
#include <iomanip>
#include <cstdlib>

#ifdef DEBUG
#define ASSERT(x) do {                                                   \
  if (!(x)) {                                                            \
    std::cout << "Failed assertion \"" #x "\" in " << __FILE__ << ':' << \
      std::dec << __LINE__ << ".\n";                                     \
    exit(1);                                                             \
  }                                                                      \
} while (0)
#else
#define ASSERT(x) do {} while (0)
#endif

#ifdef VERBOSE_DEBUG
#define DBG(x) do { \
  std::cout << x;   \
} while (0)

#define IFDBG(x) do { \
  x;                  \
} while(0)
#else
#define DBG(x) do {} while (0)
#define IFDBG(x) do {} while(0)
#endif

#endif
