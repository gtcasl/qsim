###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
QSIM_PREFIX = /usr/local
LDFLAGS = -ldl -lqsim
CXXFLAGS = -O2 -I../distorm/ -I$(QSIM_PREFIX)/include -L$(QSIM_PREFIX)/lib

EXAMPLES = qtm simple simple-client io-test

all: $(EXAMPLES)

vistrace: vistrace.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -lpthread -lImlib2 -o $@ $<

simple: simple.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< ../distorm/distorm64.a

simple-client: simple.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DQSIM_REMOTE -o $@ $< \
          ../remote/client/qsim-client.cpp ../distorm/distorm64.a

io-test: io-test.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

qtm: qtm.cpp $(QSIM_PREFIX)/lib/libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -lpthread -o $@ $< ../distorm/distorm64.a

clean:
	rm -f $(EXAMPLES) *~ *\#

