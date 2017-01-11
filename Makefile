###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
CXXFLAGS ?= -g -Wall -Idistorm/ -std=c++0x -march=native
QSIM_PREFIX ?= /usr/local
LDFLAGS = -L./
LDLIBS = -lqsim -ldl -lrt -pthread

QEMU_BUILD_DIR=build

all: libqsim.so qsim-fastforwarder

debug: CXXFLAGS += -O0
debug: BUILD_DIR = .dbg_build
release: CXXFLAGS += -O3
release: BUILD_DIR = .opt_build

statesaver.o: statesaver.cpp statesaver.h qsim.h
	$(CXX) $(CXXFLAGS) -I./ -fPIC -c -o statesaver.o statesaver.cpp

qsim-load.o: qsim-load.cpp qsim-load.h qsim.h
	$(CXX) $(CXXFLAGS) -I./ -fPIC -c -o qsim-load.o qsim-load.cpp

qsim-prof.o: qsim-prof.cpp qsim-prof.h qsim.h
	$(CXX) $(CXXFLAGS) -I./ -fPIC -c -o qsim-prof.o qsim-prof.cpp

qsim-fastforwarder: fastforwarder.cpp statesaver.o statesaver.h libqsim.so
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -I ./ -L ./ -pthread \
               -o qsim-fastforwarder fastforwarder.cpp statesaver.o $(LDLIBS)

libqsim.so: qsim.cpp qsim-load.o qsim-prof.o qsim.h qsim-vm.h mgzd.h \
            qsim-regs.h qsim-x86-regs.h qsim-arm64-regs.h
	$(CXX) $(CXXFLAGS) -shared -fPIC -o $@ $< qsim-load.o qsim-prof.o -ldl -lrt

install: libqsim.so qsim-fastforwarder qsim.h qsim-vm.h mgzd.h \
	 qsim-load.h qsim-prof.h qsim-regs.h \
	 qsim-arm-regs.h qsim-x86-regs.h qsim-arm64-regs.h qsim_magic.h
	mkdir -p $(QSIM_PREFIX)/lib
	mkdir -p $(QSIM_PREFIX)/include
	mkdir -p $(QSIM_PREFIX)/bin
	cp libqsim.so $(QSIM_PREFIX)/lib/
	cp capstone/libcapstone.so $(QSIM_PREFIX)/lib
	cp qsim.h qsim-vm.h mgzd.h qsim-load.h qsim-prof.h 		\
	 qsim-regs.h qsim-arm-regs.h qsim-x86-regs.h qsim-arm64-regs.h 	\
	 qsim_magic.h $(QSIM_PREFIX)/include/
	cp capstone/include/capstone/*.h $(QSIM_PREFIX)/include
	cp qsim-fastforwarder $(QSIM_PREFIX)/bin/
	cp $(QEMU_BUILD_DIR)/x86_64-softmmu/qemu-system-x86_64 		\
	   $(QSIM_PREFIX)/lib/libqemu-qsim-x86.so
	cp $(QEMU_BUILD_DIR)/aarch64-softmmu/qemu-system-aarch64 	\
	   $(QSIM_PREFIX)/lib/libqemu-qsim-a64.so
ifeq ($(USER),root) # Only need this if we're installing globally as root.
	/sbin/ldconfig
endif

uninstall: $(QSIM_PREFIX)/lib/libqsim.so
	rm -f $(QSIM_PREFIX)/lib/libqsim.so $(QSIM_PREFIX)/include/qsim.h \
              $(QSIM_PREFIX)/include/qsim-vm.h                            \
	      $(QSIM_PREFIX)/include/qsim-regs.h                          \
              $(QSIM_PREFIX)/include/qsim-load.h                          \
              $(QSIM_PREFIX)/include/qsim-prof.h                          \
	      $(QSIM_PREFIX)/bin/qsim-fastforwarder

.PHONY: debug

debug: all
	./build-qemu.sh $@		

.PHONY: release tests

release: all
	./build-qemu.sh $@		

tests: release install x86_tests a64_tests

x86_prep:
	if [ ! -e initrd/initrd.cpio.x86 ]; then \
		cd initrd && ./getbusybox.sh; fi

x86_tests: x86_prep
	if [ ! -e state.1 ]; then \
		./qsim-fastforwarder linux/bzImage 1 512 state.1; fi;
	cd tests/x86 && make
	cd tests && make &&			\
	./tester 1 ../state.1 x86/icount.tar && \
	diff x86/icount.out x86/icount_gold.out && \
	./tester 1 ../state.1 x86/memory.tar && \
	diff x86/memory.out x86/memory_gold.out && \
	./tester 1 ../state.1 x86/reg.tar && \
	diff x86/reg.out x86/reg_gold.out

a64_prep:
	if [ ! -e initrd/initrd.cpio.arm64 ]; then \
		cd initrd && ./getbusybox.sh arm64; fi

a64_tests: a64_prep
	if [ ! -e state.1.a64 ]; then \
		./qsim-fastforwarder linux/Image 1 512 state.1.a64 a64; fi;
	cd tests/arm64 && make
	cd tests && make &&			\
	./tester 1 ../state.1.a64 arm64/icount.tar && \
	diff arm64/icount.out arm64/icount_gold.out && \
	./tester 1 ../state.1.a64 arm64/memory.tar && \
	diff arm64/memory.out arm64/memory_gold.out
	if [ ! -e state.2.a64 ]; then \
		./qsim-fastforwarder linux/Image 2 512 state.2.a64 a64; fi;
	cd tests && make &&			\
	./tester 2 ../state.2.a64 arm64/contention.tar

clean:
	rm -f *~ \#*\# libqsim.so *.o test qtm qsim-fastforwarder build

distclean: clean
	rm -rf .dbg_build .opt_build
