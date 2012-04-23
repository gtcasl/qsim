QSIM_PREFIX ?= /usr/local

server: server.cpp ../qsim-net.h
	g++ -pthread -g -L$(QSIM_PREFIX)/lib -I$(QSIM_PREFIX)/include \
	    -o server server.cpp -ldl -lqsim

clean: 
	rm -f server *~
