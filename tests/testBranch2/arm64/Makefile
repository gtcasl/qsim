CXXFLAGS ?= -g -O2 -static -fopenmp -lpthread -std=c++0x -Wall -I$(QSIM_PREFIX)/include
LDFLAGS ?= -ldl -lpthread

TESTS = brTest

all: $(TESTS)

%: %.cpp
	aarch64-linux-gnu-g++ $(CXXFLAGS) $(LDFLAGS) -o $@ $< 
	sed 's/TEST/$@/g' ../../runme_orig.sh > runme.sh
	tar -cf $@.tar $@ runme.sh

clean:
	rm -f $(TESTS) runme.sh *.tar
