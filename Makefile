###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
CXXFLAGS = -O2 -g -Idistorm/
QSIM_PREFIX ?= /usr/local
LDFLAGS = -L$(QSIM_PREFIX)/lib
LDLIBS = -lqsim -ldl

QEMU_DIR=qemu-0.12.3

all: libqsim.so qsim-fastforwarder

statesaver.o: statesaver.cpp statesaver.h qsim.h
	$(CXX) $(CXXFLAGS) -I./ -c -o statesaver.o $(LDLIBS) statesaver.cpp

qsim-load.o: qsim-load.cpp qsim-load.h qsim.h
	$(CXX) $(CXXFLAGS) -I./ -fPIC -shared -c -o qsim-load.o qsim-load.cpp

qsim-fastforwarder: fastforwarder.cpp statesaver.o statesaver.h libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -I ./ -L ./ -pthread \
               -o qsim-fastforwarder fastforwarder.cpp statesaver.o $(LDLIBS)

libqsim.so: qsim.cpp qsim-load.o qsim.h qsim-vm.h mgzd.h qsim-regs.h 
	$(CXX) $(CXXFLAGS) -shared -fPIC -o $@ $< qsim-load.o

install: libqsim.so qsim-fastforwarder qsim.h qsim-vm.h mgzd.h qsim-regs.h \
	 qsim-load.h qsim-lock.h
	mkdir -p $(QSIM_PREFIX)/lib
	mkdir -p $(QSIM_PREFIX)/include
	mkdir -p $(QSIM_PREFIX)/bin
	cp libqsim.so $(QSIM_PREFIX)/lib/
	cp qsim.h qsim-vm.h mgzd.h qsim-regs.h qsim-load.h qsim-lock.h \
	   $(QSIM_PREFIX)/include/
	cp qsim-fastforwarder $(QSIM_PREFIX)/bin/
	cp $(QEMU_DIR)/x86_64-softmmu/qemu-system-x86_64 \
	   $(QSIM_PREFIX)/lib/libqemu-qsim.so
ifeq ($(USER),root) # Only need this if we're installing globally as root.
	/sbin/ldconfig
endif

uninstall: $(QSIM_PREFIX)/lib/libqsim.so
	rm -f $(QSIM_PREFIX)/lib/libqsim.so $(QSIM_PREFIX)/include/qsim.h     \
              $(QSIM_PREFIX)/include/qsim-vm.h                                \
	      $(QSIM_PREFIX)/include/qsim-regs.h                              \
              $(QSIM_PREFIX)/include/qsim-load.h                              \
              $(QSIM_PREFIX)/include/qsim-lock.h                              \
              $(QSIM_PREFIX)/bin/qsim-fastforwarder

clean:
	rm -f *~ \#*\# libqsim.so *.o test qtm qsim-fastforwarder
