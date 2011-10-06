#ifndef __DATA_H
#define __DATA_H

#include <map>
#include <set>
#include <string>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <sstream>

#include <stdint.h>

namespace SimpleSim {
  class Counter {
  public:
    // Default constructor for temporaries leaves ptr NULL
    Counter(): ptr(NULL) {}

    Counter(std::string name, bool resettable = true): ptr(&counterReg[name]) {
      if (resettable) resetSet.insert(ptr);
    }

    operator long long() { return *ptr; }

    Counter &operator++() { ++(*ptr); return *this; }
    Counter &operator--() { --(*ptr); return *this; }

    unsigned operator++(int) { return (*ptr)++; }
    unsigned operator--(int) { return (*ptr)--; }

    unsigned operator+=(unsigned n) { return *ptr += n; }
    unsigned operator-=(unsigned n) { return *ptr -= n; }

    static void printAll(std::ostream &os);
    static void resetAll();

  private:
    int64_t *ptr;

    static std::map<std::string, int64_t> counterReg;
    static std::set<int64_t*> resetSet;
  };

  template <typename T> std::string toStr(T &x) {
    std::ostringstream os;
    os << x;
    return os.str();
  }

  static inline double fRand() {
    int r = rand();
    return r/((double)RAND_MAX+1);
  }

  static inline unsigned uniformRand (unsigned max) {
    return (unsigned)(fRand()*max);
  }

};

#endif
