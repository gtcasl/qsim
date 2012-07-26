#ifndef __QTICKABLE_H
#define __QTICKABLE_H

#include <qcache.h>

namespace Qcache {
  class Tickable {
  public:
    virtual void tick() { ASSERT(false); }
  };
};

#endif
