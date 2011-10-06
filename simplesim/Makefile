QSIM_PREFIX=/usr/local

# Choose a CXXFLAGS. One for production, one for profiling, and one for debug.
CXXFLAGS = -O3 -I$(QSIM_PREFIX)/include
#CXXFLAGS = -g -pg -O0
#CXXFLAGS = -g -O0 -DDEBUG -DVERBOSE_DEBUG -I$(QSIM_PREFIX)/include -pthread
LDFLAGS = -L$(QSIM_PREFIX)/lib -lqsim -ldl -pthread

main : main.cpp data.o data.h cpu.o cpu.h cache.o cache.h des.o des.h

des.o : des.cpp des.h
data.o : data.cpp data.h
cache.o : cache.cpp cache.h data.h des.h debug.h
cpu.o : cpu.cpp cpu.h cache.h data.h des.h debug.h

clean:
	rm -f *.o *~ \#* main
