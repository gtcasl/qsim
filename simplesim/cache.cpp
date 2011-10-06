#include "cache.h"
#include "des.h"
#include "debug.h"

#include <stdint.h>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

const unsigned BROADCAST_INVALIDATE_COST(2);
const unsigned L1_L2_COST(10);

using namespace SimpleSim;

Cache::Cache(unsigned lvl, unsigned id, unsigned l2Linesize, unsigned l2Sets,
             unsigned ways, const char *nameAddendum) :
  name(std::string("CacheL") + toStr(lvl) + std::string(nameAddendum) 
        + std::string("-") + toStr(id)),
  l2Linesize(l2Linesize), l2Sets(l2Sets), ways(ways),
  peers(),
  array(1<<l2Sets, std::vector<Cache::Line>(ways, Cache::Line())),
  outstandingReqs(name + std::string(":outstandingReqs"), false),
  completedReqs(name + std::string(":completedReqs")),
  totalWrites(name + std::string(":totalWrites")),
  misses(name + std::string(":misses")),
  invalidates(name + std::string(":invalidates"))
{}

Cache::Line *Cache::lookup(addr_t addr, unsigned &way, unsigned &set) {
  addr_t base(addr & ~((1l<<l2Linesize)-1));

  set = (addr >> l2Linesize)&((1l<<l2Sets)-1);

  for (way = 0; way < ways; ++way) {
    Cache::Line &l(array[set][way]);
    if (l.state != 'i' && l.base == base) return &l;
  }

  return NULL;
}

void Cache::broadcastInvalidate(addr_t addr) {
  for (unsigned i = 0; i < peers.size(); ++i) {
    peers[i]->invalidateReq(addr);
  }
}

void Cache::broadcastShare(addr_t addr) {
  for (unsigned i = 0; i < peers.size(); ++i) {
    peers[i]->shareReq(addr);
  }
}

Cache::Line *Cache::evictIfNeeded(unsigned set, bool &writeBack) {
  // First, look for a free line in this set.
  for (unsigned i = 0; i < ways; i++) {
    if (array[set][i].state == 'i') {
      writeBack = false;
      return &array[set][i];
    }
  }

  // There are no free lines. Select a random line for eviction.
  unsigned evictWay = uniformRand(ways);
  Cache::Line *l = &array[set][evictWay];
  writeBack = (l->state == 'm');
  return l;
}

void Cache::req(MemReq *mr) {
  ++outstandingReqs;
  if (mr->getWr()) ++totalWrites;

  DBG(std::dec << Slide::_now << ": " << name << ": @0x" << std::hex <<
      mr->getAddr() << ": " << mr << '\n');

  // Attempt lookup.
  unsigned way, set;
  addr_t addr = mr->getAddr();
  Cache::Line *l(lookup(addr, way, set));

  if (l) {
    // Hit.
    if (mr->getWr() && l->state == 's') {
      // Write to line in shared state. Broadcast invalidate first.
      l->state = 'm';
      broadcastInvalidate(addr);
      Slide::schedule(BROADCAST_INVALIDATE_COST, this, &Cache::doResp, mr, 5);
    } else {
      // Read any line, or write to line already in modified state.
      ASSERT(mr->getWr() == false || l->state == 'm');
      Slide::schedule(0, this, &Cache::doResp, mr, 5);
    }
  } else {
    ++misses;

    // Miss. Go down one level and bring in a line in 's' state, evicting an
    // existing line if necessary.
    bool writeBack;
    Cache::Line *l(evictIfNeeded(set, writeBack));

    if (writeBack) {
      Slide::schedule(0, this, &Cache::doLowerLevelReq, 
                      new MemReq(l->base, true), 5);
    }

    // All of the other guys must have this line in the shared state (read) or 
    // invalid state (write). If they do not (in actual hardware), the contents
    // of the line would be forwarded.
    if (mr->getWr()) { broadcastInvalidate(addr); } 
    else { broadcastShare(addr); }; 

    l->state = (mr->getWr()?'m':'s');
    l->base = addr & ~((1l<<l2Linesize)-1);

    // Create the read request to get this line.
    mr->setWr(false);
    Slide::schedule(L1_L2_COST, this, &Cache::doLowerLevelReq, mr, 5);
  }
}

void Cache::doResp(MemReq *mr) { 
  --outstandingReqs;
  ++completedReqs;
  mr->resp(); 
}

void Cache::doLowerLevelReq(MemReq *mr) {
  mr->pushDev(this);
  lowerLevel->req(mr);
}

void Cache::resp(MemReq *mr) {
  // We have either read a line from main memory or finished writing a line back
  // to main memory.
  if (mr->atOrig()) {
    // A writeback has completed.
    delete mr;
  } else {
    // A read request has completed. Respond to its initiator immediately.
    DBG(std::dec << Slide::_now << ": " << name << ": " << mr << "<<\n");
    Slide::schedule(0, this, &Cache::doResp, mr, 5);
  }
}

void Cache::invalidateReq(addr_t addr) {
  unsigned way, set;
  Cache::Line *l = lookup(addr, way, set);
  if (l) {
    // We do not do a writeback if the line is modified because in real hardware
    // it would be forwarded, not written back.
    l->state = 'i';
    ++invalidates;
  }
}

void Cache::shareReq(addr_t addr) {
  unsigned way, set;
  Cache::Line *l = lookup(addr, way, set);
  if (l) {
    l->state = 's';
  }
}

void DramController::req(MemReq *mr) {
  ++outstandingReqs;
  if (mr->getWr()) ++outstandingWrites;

  DBG( std::dec << Slide::_now << ": DramController: @0x" << std::hex <<
         mr->getAddr() << ": " << mr << "--req\n");

  Slide::schedule(delayCycles, this, &DramController::doResp, mr, 5);
}

void DramController::doResp(MemReq *mr) {
  DBG( std::dec << Slide::_now << ": DramController: @0x" << std::hex <<
       mr->getAddr() << ": " << mr << "--doResp\n");

  --outstandingReqs;
  if (mr->getWr()) --outstandingWrites;
  ++completedReqs;
  if (mr->getWr()) ++completedWrites;
  mr->resp();
}
