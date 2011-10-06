#include "data.h"

#include <map>
#include <string>
#include <iostream>

std::map<std::string, int64_t> SimpleSim::Counter::counterReg;
std::set<int64_t *> SimpleSim::Counter::resetSet;
typedef std::map<std::string, int64_t>::iterator CRegIt;
typedef std::set<int64_t*>::iterator ResetIt;

void SimpleSim::Counter::resetAll() {
  for (ResetIt i = resetSet.begin(); i != resetSet.end(); i++) **i = 0;
}

void SimpleSim::Counter::printAll(std::ostream &os) {
  for (CRegIt i = counterReg.begin(); i != counterReg.end(); i++) {
    os << i->first << ", " << i->second << '\n';
  }
}
