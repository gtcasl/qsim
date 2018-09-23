CXXFLAGS ?= -g -O2 -std=c++0x -Wall -I$(QSIM_PREFIX)/include -Wl,--no-as-needed
LDFLAGS ?= -L$(QSIM_PREFIX)/lib
LDLIBS ?= -pthread -ldl -lqsim -lrt

TESTS = tester

all: $(TESTS)

cs_disas.o : cs_disas.cpp cs_disas.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c cs_disas.h cs_disas.cpp $< $(LDLIBS) -lcapstone

%: %.cpp cs_disas.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) cs_disas.o -o $@ $< $(LDLIBS) -lcapstone
clean:
	rm -f *.o $(TESTS)
