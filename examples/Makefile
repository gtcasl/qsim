###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
QSIM_PREFIX ?= /usr/local
CXXFLAGS ?= -g -O0 -I../distorm/ -I$(QSIM_PREFIX)/include -L$(QSIM_PREFIX)/lib
LDLIBS ?= -lqsim -pthread -ldl

EXAMPLES = qtm simple io-test cachesim

all: $(EXAMPLES)

vistrace: vistrace.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS) -lImlib2

simple: simple.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< ../distorm/distorm64.a $(LDLIBS)

io-test: io-test.cpp $(QSIM_PREFIX)/lib/libqsim.so ../distorm/distorm64.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< ../distorm/distorm64.a $(LDLIBS)

qtm: qtm.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -lpthread -o $@ $< ../distorm/distorm64.a $(LDLIBS)

clean:
	rm -f $(EXAMPLES) *~ *\#

