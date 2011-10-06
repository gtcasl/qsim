#ifndef __CPU_H
#define __CPU_H

#include "des.h"
#include "data.h"
#include "debug.h"
#include "cache.h"

#include <stdint.h>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <queue>

#include <qsim.h>

namespace SimpleSim {
  class Cpu : public MemSysDevice {
  public:
    void req(MemReq *mr) { throw "Made memory request to CPU."; }
    virtual void resp(MemReq *mr) = 0;
  };

  // Does sequential accesses uniformly distributed between simultaneous and
  // 2mtba cycles apart. With probability probChangeBase of starting at a new
  // random address instead of continuing the stream of accesses.
  class RandomCpu : public Cpu {
  public:
    RandomCpu(unsigned id, double m, double p, unsigned maxOutstanding);

    virtual void resp(MemReq *mr);

    void setLowerLevel(MemSysDevice *ll) { lowerLevel = ll; }

  private:
    void nextAccess(addr_t *a);

    std::string name;
    double probChangeBase;
    double mtba;
    Counter outstandingReqs, completedReqs;
    unsigned maxOutstandingReqs;
    MemSysDevice *lowerLevel;
  };

  // 1IPC, stalls on memory requests.
  class SimpleCpu : public Cpu {
  public:
    SimpleCpu(unsigned id, Qsim::OSDomain &osd);
    void resp(MemReq *mr);

    void instCB(uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
                enum inst_type t);
    void memCB(uint64_t v, uint64_t p, uint8_t s, int t);
    void regCB(int r, uint8_t s, int t) {}
    void intCB(uint8_t v) {}

    void setCache(MemSysDevice *i, MemSysDevice *d) { iCache = i; dCache = d; }

  private:
    void next(SimpleCpu *);
    void timer(SimpleCpu *);

    unsigned id;
    std::string name;
    Counter outstandingReqs, completedReqs, committedInsts, idleInsts;
    MemSysDevice *iCache, *dCache;
    bool cpuRan;
  };

  // Out-of-order CPU back-end (from issue to commit)
  class SuperscalarCpu : public Cpu {
  public:
    SuperscalarCpu(unsigned id, Qsim::OSDomain &osd, unsigned robSize,
                   unsigned issuesPerCycle, unsigned retiresPerCycle);

    void addFu(enum inst_type t, unsigned n,
               unsigned r, unsigned l, unsigned ii)
    {
      fuMap[t] = FuMapEntry(t, n, r, l, ii, name);
    }

    void setCache(MemSysDevice *i, MemSysDevice *d) { iCache = i; dCache = d; }

    void instCB(uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
                enum inst_type t);
    void memCB(uint64_t v, uint64_t p, uint8_t s, int w);
    void regCB(int r, uint8_t s, int type);
    void intCB(uint8_t c);

    void resp(MemReq *mr);

  private:
    struct RobEntry;
    struct UOp;

    void doRetire();                   // Priority 10
    void doFetch();                    // Priority 15
    void doIssue();                    // Priority 17
    void rsWait(UOp* uOp);             // Priority 30
    void opComplete(RobEntry *dest);   // Priority 20

    unsigned id;
    std::string name;

    unsigned issuesPerCycle, retiresPerCycle, issuesLeft;
    MemReq *fetchReq;

    // The microinstruction format:
    //  - Operation type: (QSIM_INST_NULL for ld/store or reg/reg move)
    //  - Bit vectors of register sources, flag sources, and destination flags.
    //  - Destination register
    //  - Memory operation info
    struct UOp {
      UOp() : 
        type(QSIM_INST_NULL), rSrcVec(0), fSrcVec(0), fDstVec(0), rDstVec(0),
          memOp(false), wr(false), lastInInst(false), mr(NULL), addr(0),
          sources(), dest(NULL) {}
      enum inst_type type;
      unsigned rSrcVec, fSrcVec, fDstVec, rDstVec;
      bool memOp, wr, lastInInst;
      MemReq *mr;
      uint64_t addr;
      std::set<RobEntry *> sources;
      RobEntry *dest;
    };
    UOp nextUOp;
    std::queue<UOp> uOpQueue;

    uint64_t fetchAddr;

    // The re-order buffer/architectural register file.
    struct RobEntry {
      RobEntry(): valid(false), ratEntry(), fatEntry(), lastInInst(false), 
                  type(QSIM_INST_NULL) {}
      
      void retire() { 
        valid=false;
        for (RESetIt i = ratEntry.begin(); i != ratEntry.end(); ++i) **i = NULL;
        for (RESetIt i = fatEntry.begin(); i != fatEntry.end(); ++i) **i = NULL;
      }

      bool valid;
      std::set<RobEntry **> ratEntry, fatEntry;
      typedef std::set<RobEntry **>::iterator RESetIt;
      bool lastInInst;
      enum inst_type type;
    };

    unsigned robHead, robTail;
    Counter robOccupancy;
    std::vector<RobEntry> rob;  // The actual ROB data structure.
    std::vector<RobEntry*> rat; // Register alias table
    std::vector<RobEntry*> fat; // Flag alias table

    std::map<MemReq*, UOp*> memOps; // Outstanding memory operations.

    Counter retiredOps, fetchedInsts;

    // The fuMap maps operation types to functional units and provides a way to
    // determine if units are available and whether instructions can be issued.
    struct FuMapEntry {
      // Default constructor for unnecessary temporaries.
      FuMapEntry() {}

      FuMapEntry(enum inst_type t, unsigned n, unsigned r, unsigned l, 
                 unsigned ii, std::string pName) :
        nAvailable(n), rsAvailable(r), latency(l), initiationInterval(ii),
        nTotal(n), nIIdx(0),
        issuedOps(pName + std::string("-issuedOps-") + 
                  std::string(itypeTable[t])),
        lastInit(n)
      {}

      unsigned nAvailable, rsAvailable, latency, initiationInterval, nTotal, 
               nIIdx;
      std::vector<uint64_t> lastInit;
      Counter issuedOps;
    };
    std::map <enum inst_type, FuMapEntry> fuMap;

    MemSysDevice *iCache, *dCache;
    bool inInst;

    void timer(SuperscalarCpu*);

    static const char *itypeTable[];

    static const unsigned TMP_REG;

    static std::ostream &print(std::ostream &, const SuperscalarCpu::UOp&);
  };
};

#endif
