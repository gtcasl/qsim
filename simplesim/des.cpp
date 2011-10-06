// Build this for the stand-alone DES.

#include "des.h"
#include <map>
#include <stdint.h>

uint64_t Slide::_now = 0;
bool Slide::_terminated(false);
std::multimap<uint64_t, Slide::_event_t*> Slide::_event_q;
std::vector <Slide::_clock_t> Slide::_clocks;

