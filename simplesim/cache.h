#ifndef __CACHE_H
#define __CACHE_H

#include "des.h"
#include "data.h"
#include "debug.h"

#include <stdint.h>
#include <vector>
#include <map>
#include <stack>

namespace SimpleSim {
  typedef uint64_t addr_t;

  class MemReq;

  class MemSysDevice {
  public:
    virtual ~MemSysDevice() {}

    virtual void req(MemReq *mr) = 0;
    virtual void resp(MemReq *mr) = 0;
  };

  class MemReq {
  public:
    MemReq(addr_t addr, bool wr): addr(addr), wr(wr), respStack() {}

    void pushDev(MemSysDevice *d) { respStack.push(d); }

    void resp() {
      ASSERT(!respStack.empty());
      MemSysDevice *msd(respStack.top());
      respStack.pop();
      msd->resp(this);
    }

    addr_t getAddr() { return addr; }
    bool getWr() { return wr; }
    void setWr(bool w) { wr = w; }
    bool atOrig() { return respStack.empty(); }
    bool operator==(MemReq &r) { return (addr == r.addr) && (wr == r.wr); }

  private:
    addr_t addr;
    bool wr;
    std::stack <MemSysDevice*> respStack;
  };

  class Cache : public MemSysDevice {
  public:
    Cache(unsigned lvl, unsigned id, unsigned l2Linesize, 
          unsigned l2Sets, unsigned ways, const char *nameaddendum="U");

    void addPeer(Cache* p) { peers.push_back(p); }
    void setLowerLevel(MemSysDevice *ll) { lowerLevel = ll; }

    virtual void req(MemReq *mr);
    virtual void resp(MemReq *mr);

  private:
    struct Line;

    Line *lookup(addr_t addr, unsigned &set, unsigned &way);
    void broadcastInvalidate(addr_t addr);
    void broadcastShare(addr_t addr);

    void invalidateReq(addr_t addr);
    void shareReq(addr_t addr);

    void doResp(MemReq *mr);
    void doLowerLevelReq(MemReq *mr);

    Line *evictIfNeeded(unsigned set, bool &writeBack);

    struct Line {
    Line() : state('i') {}
      addr_t base;
      char state;  // 'm', 's', or 'i'
    };

    std::string name;
    unsigned l2Linesize, l2Sets, ways;
    std::vector<Cache*> peers;
    std::vector<std::vector <Line> > array;
    Counter outstandingReqs, completedReqs, totalWrites, misses, invalidates;
    MemSysDevice *lowerLevel;
  };

  class DramController : public MemSysDevice {
  public:
    DramController(unsigned d):
      delayCycles(d),
      outstandingReqs(std::string("DramController:outstandingReqs"), false),
      completedReqs(std::string("DramController:completedReqs")),
      outstandingWrites(std::string("DramController:outstandingWrites"), false),
      completedWrites(std::string("DramController:completedWrites")) {}

    void req(MemReq *mr);
    void resp(MemReq *mr) { throw "Sent memory response to DramController."; }

  private:
    void doResp(MemReq *mr);

    Counter outstandingReqs, completedReqs, outstandingWrites, completedWrites;
    unsigned delayCycles;
  };

};

#endif
