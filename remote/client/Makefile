client: client.cpp qsim-client.cpp qsim-client.h ../qsim-net.h
	g++ -o client -L/usr/local -I/usr/local/include client.cpp \
	  qsim-client.cpp -ldl -lqsim

clean:
	rm -f client *~
