#include <iostream>
#include <cstdlib>

#include "des.h"

using Slide::schedule;

class juicematic {
public:
  juicematic(): fruits_left(10)            {}
  juicematic(unsigned n) : fruits_left(n)  {}

  void rest(void *v) {
    if (fruits_left > 0) {
      // Squeeze a random fraction of my fruits a random amount of time in the
      // future.
      unsigned fruits_to_squeeze = rand()%(fruits_left) + 1;
      schedule(rand()%1000, 
	       this, 
	       &juicematic::squeeze_some, 
	       new unsigned(fruits_to_squeeze));
    } else {
      std::cout << "Juicematic " << this << " has no fruits left.\n";
    }
  }

  void squeeze_some(unsigned *n) {
    if (fruits_left >= *n) {
      fruits_left -= *n;
      std::cout << "Juicematic " << this << " squeezing " 
		<< *n << " fruits.\n";
      // That squeezin' was hard work. Rest for a little while.
      schedule(rand()%1000, this, &juicematic::rest, (void*)0);
    } else {
      std::cerr << "Wanted to squeeze " << *n << " fruits, but I only have "
	        << fruits_left << "!\n";
      // Don't squeeze anymore.
    }

    delete n;
  }

private:
  unsigned fruits_left;
};

int main() {
  juicematic j(101), k(102), l(103);

  schedule(rand()%1000, &j, &juicematic::rest, (void*)0);
  schedule(rand()%1000, &k, &juicematic::rest, (void*)0);
  schedule(rand()%1000, &l, &juicematic::rest, (void*)0);

  while(Slide::_tick());

  return 0;
}
