server: server.cpp ../qsim-net.h
	g++ -pthread -g -L/usr/local/lib -I/usr/local/include -ldl -lqsim \
	    -o server server.cpp

clean: 
	rm -f server *~
