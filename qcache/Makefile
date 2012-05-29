###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
QSIM_PREFIX ?= /usr/local
CXXFLAGS ?= -O2 -I../distorm/ -I$(QSIM_PREFIX)/include -L$(QSIM_PREFIX)/lib
LDLIBS ?= -lqsim -pthread -ldl

main: qcache.o main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ qcache.o main.o $(LDLIBS)

clean:
	rm -f qcache.o main.o main
