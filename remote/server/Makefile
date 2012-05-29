QSIM_PREFIX ?= /usr/local

qsim-server: server.cpp ../qsim-net.h
	g++ -pthread -g -L$(QSIM_PREFIX)/lib -I$(QSIM_PREFIX)/include -I../ \
	    -o qsim-server server.cpp -ldl -lqsim

install: qsim-server
	cp qsim-server $(QSIM_PREFIX)/bin

clean: 
	rm -f qsim-server *~
