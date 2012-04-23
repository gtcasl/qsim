QSIM_PREFIX ?= /usr/local

client: client.cpp libqsim-client.so
	g++ -o client -L/usr/local -L. -I/usr/local/include -I../ client.cpp \
	       qsim-client.cpp -ldl -lqsim -lqsim-client

libqsim-client.so: qsim-client.cpp qsim-client.h ../qsim-net.h
	g++ -fPIC -o libqsim-client.so -shared qsim-client.cpp -I ../

install: client libqsim-client.so
	cp libqsim-client.so $(QSIM_PREFIX)/lib
	cp qsim-client.h $(QSIM_PREFIX)/include
	cp ../qsim-net.h $(QSIM_PREFIX)/include

clean:
	rm -f client libqsim-client.so *~
