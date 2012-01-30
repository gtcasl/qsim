###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
CXXFLAGS = -O2 -g -Idistorm/
PREFIX = /usr/local
LDFLAGS = -L$(PREFIX)/lib
LDLIBS = -lqsim -ldl

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

install: libqsim.so qsim-fastforwarder
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	mkdir -p $(PREFIX)/bin
	cp libqsim.so $(PREFIX)/lib/
	cp qsim.h qsim-vm.h mgzd.h qsim-regs.h qsim-load.h $(PREFIX)/include/
	cp qsim-fastforwarder $(PREFIX)/bin/
ifeq ($(USER),root) # Only need this if we're installing globally as root.
	/sbin/ldconfig
endif

uninstall: $(PREFIX)/lib/libqsim.so
	rm -f $(PREFIX)/lib/libqsim.so $(PREFIX)/include/qsim.h         \
              $(PREFIX)/include/qsim-vm.h $(PREFIX)/include/qsim-regs.h \
              $(PREFIX)/bin/qsim-fastforwarder

clean:
	rm -f *~ \#*\# libqsim.so *.o test qtm qsim-fastforwarder
