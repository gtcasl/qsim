CXXFLAGS ?= -g -O2 -static -std=c++0x -Wall -I$(QSIM_PREFIX)/include
LDLIBS ?= -lpthread -ldl

TESTS = icount memory reg

all: $(TESTS)

%: %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
	sed 's/TEST/$@/g' ../runme_orig.sh > runme.sh
	tar -cf $@.tar $@ runme.sh

clean:
	rm -f $(TESTS) runme.sh *.tar
