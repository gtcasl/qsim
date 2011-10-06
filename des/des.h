#ifndef __SLIDE_DES_H
#define __SLIDE_DES_H

#include <map>

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
#ifdef MANIFOLD
  template <typename T> static inline void schedule(unsigned cycles,
						    void(*f)(T*),
						    T* arg)
  {
#error schedule() not implemented for Manifold.
  }
#else 
#ifdef SST
#else /* Standalone serial DES : */

  uint64_t _now = 0;

  struct _event_t { virtual ~_event_t() {} };
  template <typename S, typename T> struct _event_spec_t: public _event_t { 
    _event_spec_t(S* s, void (S::*f)(T*), T* a): s(s), f(f), a(a) {}
    virtual ~_event_spec_t() { ((s)->*(f))(a); }
    S* s; void (S::*f)(T*); T* a;
  };

  std::multimap<uint64_t, _event_t*> _event_q;
  static inline bool _tick() {
    if (_event_q.empty()) return false;
    _now = _event_q.begin()->first;
    delete _event_q.begin()->second; 
    _event_q.erase(_event_q.begin()); 
    return true; 
  }

  static inline bool _advance(uint64_t cycles) {
    _now += cycles;
    while (!_event_q.empty() && _event_q.begin()->first <= _now && _tick());
    return !_event_q.empty();
  }

  template <typename S, typename T> 
    static inline void schedule(unsigned cycles, 
				S* s, 
				void (S::*f)(T*), 
				T* arg)
  {
    _event_q.insert( std::pair<uint64_t,_event_t*>(_now+cycles, 
						   new _event_spec_t
						   <S, T>(s, f, arg)));
  }
#endif 
#endif
};
#endif /*__SLIDE_DES_H*/
