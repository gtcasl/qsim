server: server.cpp ../qsim-net.h
	g++ -pthread -g -L/usr/local/lib -I/usr/local/include \
	    -o server server.cpp -ldl -lqsim

clean: 
	rm -f server *~
