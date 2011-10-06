#ifndef __SLIDE_DES_H
#define __SLIDE_DES_H
#include "debug.h"

#include <map>
#include <vector>
#include <algorithm>

#include <stdint.h>

//   schedule(cycles, S* c, void(*f)(T*), T* arg)
//
// This is the single function that must be implemented in order to port the
// Slide libraries to your simulation infrastructure of choice. A very simple
// serial discrete event simulator is included for both testing and simple
// standalone simulations. The only thing it can do is call single-argument
// class member functions at arbitrary times in the future.
//
// The following define the control mechanism for the built-in DES. Other DES
// systems will have their own main loops to control advancement.
//
// bool _tick();
//
// Advance the builtin DES to the next event and execute it. Returns true if
// more events remain to process.
//
// bool _advance(uint64_t n);
//
// Advance the builtin DES by n cycles. Returns true if more events remain to
// process.

namespace Slide {
#ifdef SLIDE_MANIFOLD
#error Slide DES not implemented for Manifold.
#else 
#ifdef SLIDE_SST
#error Slide DES not implemented for SST.
#else /* Standalone serial DES : */

  extern uint64_t _now;

  struct _mptr_t {
    virtual ~_mptr_t() {}
    virtual void operator()()=0;
  };

  class _clock_t {
  public:
    _clock_t(uint8_t p, _mptr_t *f):p(p), func(f) {}
    void tick() { (*func)(); }
    bool operator <(_clock_t const &r) const { return p < r.p; }
    uint8_t priority() { return p; }

  private:
    _mptr_t *func;
    uint8_t p;
  };

  template <typename T> class _mptr_spec_t : public _mptr_t {
  public:
    _mptr_spec_t(T *c, void (T::*f)()): c(c), f(f) {}
    void operator()() { ((c)->*(f))(); }
  private:
    T* c;
    void (T::*f)();
  };

  extern std::vector<_clock_t> _clocks;

  template <typename T> void reg_clock(uint8_t p, T* c, void (T::*f)()) {
    _clocks.push_back(_clock_t(p, new _mptr_spec_t<T>(c, f)));
    sort(_clocks.begin(), _clocks.end());
  }

  struct _event_t { virtual ~_event_t() {} };
  template <typename S, typename T> struct _event_spec_t: public _event_t { 
    _event_spec_t(S* s, void (S::*f)(T*), T* a): s(s), f(f), a(a) {}
    virtual ~_event_spec_t() { ((s)->*(f))(a); }
    S* s; void (S::*f)(T*); T* a;
  };
  template <typename S> struct _event2_spec_t: public _event_t {
    _event2_spec_t(S* s, void (S::*f)()): s(s), f(f) {}
    virtual ~_event2_spec_t() { ((s)->*(f))(); }
    S* s; void (S::*f)();
  };

  extern bool _terminated;
  extern std::multimap<uint64_t, _event_t*> _event_q;

  static inline bool _tick() {
    if (_terminated) return false;

    uint64_t start_now(_now);
    unsigned next_clock(0);

    while (_now == start_now) {
      bool consider_clocks(next_clock != _clocks.size());
      bool consider_events(!_event_q.empty() && (_event_q.begin()->first >> 8) == _now);

      if (!consider_clocks && !consider_events) {
        DBG("Advancing time.\n");
        ++_now;
      } else if (!consider_clocks || _event_q.begin()->first & 0xff < 
                                       _clocks[next_clock].priority()) 
      {
        DBG("Running event.\n");
        std::multimap<uint64_t, _event_t*>::iterator i(_event_q.begin());
        delete i->second;
        _event_q.erase(i);
      } else {
        DBG("Ticking clock.\n");
        _clocks[next_clock++].tick();
      }
    }
    return !_terminated;
  }

  static inline bool _advance(uint64_t cycles) {
    while (_now <= cycles && _tick());
    return !_terminated;
  }

  template <typename S, typename T> 
    static inline void schedule(unsigned cycles, 
				S* s, 
				void (S::*f)(T*), 
				T* arg, uint8_t priority=128)
  {
    _event_spec_t<S, T>*e = new _event_spec_t<S, T>(s, f, arg);
    DBG("Created new event at " << e << '\n');
    _event_q.insert( std::pair<uint64_t,_event_t*>(((_now+cycles)<<8)|priority, 
						   e));
  }

  template <typename T>
    static inline void schedule(unsigned cycles,
                                T* s,
                                void (T::*f)(),
                                uint8_t priority=128)
  {
    _event2_spec_t<T>*e = new _event2_spec_t<T>(s, f);
    DBG("Created new argless event at " << e << '\n');
    _event_q.insert( std::pair<uint64_t,_event_t*>(((_now+cycles)<<8)|priority,
                                                   e));
  }

  static inline void _terminate() { _terminated = true; }
#endif 
#endif
};
#endif /*__SLIDE_DES_H*/
