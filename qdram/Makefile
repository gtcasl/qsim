###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
QSIM_PREFIX ?= /usr/local
#CXXFLAGS ?= -g -O3 -DENABLE_ASSERTIONS -I$(QSIM_PREFIX)/include -L$(QSIM_PREFIX)/lib -I../qcache
CXXFLAGS ?= -g -O0 -DDEBUG -DENABLE_ASSERTIONS -I$(QSIM_PREFIX)/include \
            -L$(QSIM_PREFIX)/lib -I../qcache
LDLIBS ?= -lqsim -pthread -ldl

qdram: main.o ../qcache/qcache.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ ../qcache/qcache.o main.o $(LDLIBS)

main.o: main.cpp qdram.h qdram-sched.h qdram-sched-fcfs.h qdram-event.h qdram-config.h

clean:
	rm -f main.o qdram
