#ifndef __QDRAM_H
#define __QDRAM_H

#include <qcache.h>

#include "qdram-event.h"

namespace Qcache {

  enum DramBankState {
    ST_IDLE, ST_ACTIVE, ST_PRECHARGE_PDN, ST_ACTIVE_PDN
  };

  template <typename TIMING_T, typename DIM_T,
            template<typename> class ADDRMAP_T>
  class Bank {
  public:
    Bank(cycle_t &now): state(ST_IDLE), now(now) {}

    void tick() { eq.apply(now); }

    // Constraint checks
    bool canActivate() { return state == ST_IDLE && eq.check(CAN_ACTIVATE); }
    bool canPrecharge() { return state == ST_ACTIVE && eq.check(CAN_PRECHARGE); }
    bool canPrechargeAll() { return state == ST_IDLE || canPrecharge(); }

    bool canRead(addr_t a) {
      return state == ST_ACTIVE && rowHit(a) && eq.check(CAN_WRITE);
    }

    bool canWrite(addr_t a) {
      return state == ST_ACTIVE && rowHit(a) && eq.check(CAN_READ);
    }

    bool canExitPowerdown() {
      return state == ST_ACTIVE_PDN || state == ST_PRECHARGE_PDN;
    }
    
    bool rowHit(addr_t a) { return row == m.getRow(a); }

    bool mustPrecharge(addr_t a) { return !rowHit(a) && state == ST_ACTIVE; }

    // Commands
    void issueActivate(addr_t a) {
      eq.sched(now + t.tRCD(), CAN_WRITE);
      eq.sched(now + t.tRCD(), CAN_READ);
      eq.sched(now + t.tRRD(), CAN_ACTIVATE);
      eq.sched(now + t.tRAS(), CAN_PRECHARGE);

      state = ST_ACTIVE;
      row = m.getRow(a);
    }

    void issueRead(addr_t a) {
      eq.sched(now + t.tRTP(), CAN_PRECHARGE);
    }

    void issueWrite(addr_t a) {
      eq.sched(now + t.tWR(), CAN_PRECHARGE);
    }

    void issuePrecharge(addr_t a) {
      eq.sched(now + t.tRP(), CAN_ACTIVATE);

      state = ST_IDLE;
    }

    bool issuePowerdownEnter(bool fastExit) {
      if (state == ST_ACTIVE) {
        state = ST_ACTIVE_PDN;
        return true;
      } else {
        state = ST_PRECHARGE_PDN;
        return fastExit;
      }
    }

  private:
    TIMING_T t;
    ADDRMAP_T<DIM_T> m;
    EventQueue<TIMING_T> eq;
    DramBankState state;
    int row; // The currently-open row.
    cycle_t &now;
  };

  template <typename TIMING_T, typename DIM_T,
            template<typename> class ADDRMAP_T>
  class Rank {
  public:
    Rank(cycle_t &now):
      banks(m.d.banks(), Bank<TIMING_T, DIM_T, ADDRMAP_T>(now)),
      dllOn(false), now(now) {}

    void tick() {
      eq.apply(now);
      for (unsigned i = 0; i < m.d.banks(); ++i) banks[i].tick();
    }

    // Constraint checks
    bool canActivate(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      return eq.check(CAN_ACTIVATE) && eq.fawCheck() && bank.canActivate();
    }

    bool canRead(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      return eq.check(CAN_READ) && bank.canRead(a);
    }

    bool canWrite(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      return eq.check(CAN_WRITE) && bank.canWrite(a);
    }

    bool canPrecharge(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      return eq.check(CAN_PRECHARGE) && bank.canPrecharge();
    }

    bool canPrechargeAll() {
      if (!eq.check(CAN_PRECHARGE)) return false;

      for (unsigned i = 0; i < m.d.banks(); ++i) {
        Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[i]);
        if (!bank.canPrechargeAll()) return false;
      }

      return true;
    }

    bool canRefresh() {
      for (unsigned i = 0; i < m.d.banks(); ++i) {
        Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[i]);
        if (!bank.canActivate()) return false;
      }

      return true;
    }

    bool canPowerdown() {
      return eq.check(CAN_PDN);
    }

    bool canExitPowerdown() {
      for (unsigned i = 0; i < m.d.banks(); ++i) {
        Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[i]);
        if (!bank.canExitPowerdown()) return false;
      }

      return true;
    }

    bool rowHit(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      return bank.rowHit(a);
    }

    bool mustPrecharge(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      return bank.mustPrecharge(a);
    }

    // Commands
    void issueActivate(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);
      
      eq.sched(now + t.tFAW(), FAW_INC);
      eq.sched(now + t.tACTPDEN(), CAN_PDN);

      bank.issueActivate(a);
    }

    void issueRead(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);

      eq.sched(now + t.tRDPDEN(), CAN_PDN);

      bank.issueRead(a);
    }

    void issueWrite(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);

      eq.sched(now + t.tWRPDEN(), CAN_PDN);

      bank.issueWrite(a);
    }

    void issuePrecharge(addr_t a) {
      Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[m.getBank(a)]);

      eq.sched(now + t.tPRPDEN(), CAN_PDN);

      bank.issuePrecharge(a);
    }

    void issuePrechargeAll() {
      eq.sched(now + t.tPRPDEN(), CAN_PDN);

      for (unsigned i = 0; i < m.d.banks(); ++i) {
        Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[i]);
        bank.issuePrecharge();
      }
    }

    void issueRefresh() {
      eq.sched(now + t.tRFC(), CAN_ACTIVATE);
    }

    bool issuePowerdownEnter(bool fastExit) {
      eq.enterPdn();

      for (unsigned i = 0; i < m.d.banks(); ++i) {
        Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[i]);
        if (!bank.issuePowerdownEnter(fastExit)) fastExit = false;
      }

      dllOn = !fastExit;
      return fastExit;
    }

    void issuePowerdownExit() {
      eq.exitPdn();

      cycle_t tEXIT = (dllOn?t.tXP():t.tXPDLL());
      dllOn = true;

      eq.sched(now + tEXIT, CAN_READ);
      eq.sched(now + tEXIT, CAN_WRITE);
      eq.sched(now + tEXIT, CAN_ACTIVATE);
      eq.sched(now + tEXIT, CAN_PRECHARGE);

      for (unsigned i = 0; i < m.d.banks(); ++i) {
        Bank<TIMING_T, DIM_T, ADDRMAP_T> &bank(banks[i]);
        bank.issuePowerdownExit();
      }
    }

  private:
    TIMING_T t;
    ADDRMAP_T<DIM_T> m;
    std::vector<Bank<TIMING_T, DIM_T, ADDRMAP_T> > banks;
    EventQueue<TIMING_T> eq;
    bool dllOn;
    cycle_t &now;
  };

  template <typename TIMING_T, typename DIM_T,
            template<typename> class ADDRMAP_T>
  class Channel {
  public:
    Channel(): ranks(m.d.ranks(), Rank<TIMING_T, DIM_T, ADDRMAP_T>(now)) {}

    // Recursively advance time in all ranks, banks.
    void tickBegin() {
      eq.apply(now);
      for (unsigned i = 0; i < ranks.size(); ++i) ranks[i].tick();
    }

    void tickEnd() { ++now; }

    // Constraint checks
    bool canActivate(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) &&
             eq.check(CAN_ACTIVATE) &&
             rank.canActivate(a);
    }

    bool canRead(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) &&
 	     eq.check(CAN_READ) &&
	     rank.canRead(a);
    }

    bool canWrite(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) &&
 	     eq.check(CAN_WRITE) &&
	     rank.canWrite(a);
    }

    bool canPrecharge(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) && rank.canPrecharge(a);
    }

    bool canPrechargeAll(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) && rank.canPrechargeAll(a);
    }

    bool canPrechargeAllAll() {
      if (!eq.check(CAN_USE_BUS)) return false;

      for (unsigned i = 0; i < m.d.ranks(); ++i) {
        Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[i]);
        if (!rank.canPrechargeAll()) return false;
      }

      return true;
    }

    bool canRefresh(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) && rank.canRefresh();
    }

    bool canRefreshAll() {
       if (!eq.check(CAN_USE_BUS)) return false;

      for (unsigned i = 0; i < m.d.ranks(); ++i) {
        Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[i]);
        if (!rank.canRefresh()) return false;
      }

      return true;
    }

    bool canPowerdown(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) && rank.canPowerdown();
    }

    bool canExitPowerdown(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);

      return eq.check(CAN_USE_BUS) && rank.canExitPowerdown();
    }

    bool rowHit(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);
      return rank.rowHit(a);
    }

    bool mustPrecharge(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);
      return rank.mustPrecharge(a);
    }

    // Commands
    void issueActivate(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);
      ASSERT(canActivate(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      rank.issueActivate(a);
    }

    void issueRead(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);
      ASSERT(canRead(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      eq.sched(now + t.tCCD(), CAN_READ);
      eq.sched(now + t.tRTW(), CAN_WRITE);
      rank.issueRead(a);
    }

    void issueWrite(addr_t a) {
      ASSERT(canWrite(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      eq.sched(now + t.tWTR(), CAN_READ);
      eq.sched(now + t.tCCD(), CAN_WRITE);
      ranks[m.getRank(a)].issueWrite(a);
    }

    void issuePrecharge(addr_t a) {
      Rank<TIMING_T, DIM_T, ADDRMAP_T> &rank(ranks[m.getRank(a)]);
      ASSERT(canPrecharge(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      rank.issuePrecharge(a);
    }

    void issuePrechargeAll(addr_t a) {
      ASSERT(canPrechargeAll(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      ranks[m.getRank(a)].issuePrechargeAll();
    }

    void issuePrechargeAllAll() {
      ASSERT(canPrechargeAllAll());

      eq.sched(now + t.tCPD(), CAN_USE_BUS);

      for (unsigned i = 0; i < m.d.ranks(); ++i) ranks[i].prechargeAll();
    }

    void issueRefresh(addr_t a) {
      ASSERT(canRefresh(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      ranks[m.getRank(a)].issueRefresh();
    }
 
    void issueRefreshAll() {
      ASSERT(canRefreshAll());

      eq.sched(now + t.tCPD(), CAN_USE_BUS);

      for (unsigned i = 0; i < m.d.ranks(); ++i) ranks[i].refresh();
    }

    bool issuePowerdownEnter(addr_t a, bool fastExit) {
      ASSERT(canPowerdown(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      return ranks[m.getRank(a)].issuePowerdownEnter(fastExit);
    }

    void issuePowerdownExit(addr_t a) {
      ASSERT(canExitPowerdown(a));
      eq.sched(now + t.tCPD(), CAN_USE_BUS);
      ranks[m.getRank(a)].issuePowerdownExit(); 
    }

  private:
    TIMING_T t;
    ADDRMAP_T<DIM_T> m;
    std::vector<Rank<TIMING_T, DIM_T, ADDRMAP_T> > ranks;
    EventQueue<TIMING_T> eq;
    cycle_t now;
  };

};

#endif
