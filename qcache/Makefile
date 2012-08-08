###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
QSIM_PREFIX ?= /usr/local
CXXFLAGS ?= -g -O3 -DENABLE_ASSERTIONS -I$(QSIM_PREFIX)/include \
            -L$(QSIM_PREFIX)/lib -I./ -I../qdram
#CXXFLAGS ?= -g -O0 -DDEBUG -DENABLE_ASSERTIONS -I$(QSIM_PREFIX)/include \
#            -L$(QSIM_PREFIX)/lib -I./ -I../qdram
LDLIBS ?= -lqsim -pthread -ldl

qcache: qcache.o main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ qcache.o main.o $(LDLIBS)

main.o: qcache.h qcache-msi.h qcache-mesi.h qcache-dir.h qcache-repl.h \
        qcache-bloom.h qcpu.h ../qdram/qdram.h ../qdram/qdram-config.h \
        ../qdram/qdram-sched.h ../qdram/qdram-event.h

qcache.o: qcache.cpp qcache.h

clean:
	rm -f qcache.o main.o qcache
