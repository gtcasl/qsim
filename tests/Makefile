CXXFLAGS ?= -g -O2 -std=c++0x -Wall -I$(QSIM_PREFIX)/include
LDFLAGS ?= -L$(QSIM_PREFIX)/lib
LDLIBS ?= -lpthread -ldl -lqsim -lrt

TESTS = tester

all: $(TESTS)

%: %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TESTS)
