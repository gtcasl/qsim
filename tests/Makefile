CXXFLAGS ?= -g -O2 -std=c++0x -Wall -I$(QSIM_PREFIX)/include -Wl,--no-as-needed
LDFLAGS ?= -L$(QSIM_PREFIX)/lib
LDLIBS ?= -pthread -ldl -lqsim -lrt

TESTS = tester

all: $(TESTS)

%: %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TESTS)
