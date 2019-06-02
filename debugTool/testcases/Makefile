#_____________STATIC STUFF__________________________________________
OPTIONS =  -std=c++11 
FILES := test.o $(shell echo ../src/*.cpp  | sed -e 's/cpp/o/g')
UNAME := $(shell uname)
EXE   := $(UNAME)_TestCases
CC := g++

INCPATH := `pkg-config gmodule-2.0 --cflags` -I include -I ../include
LIBPATH := `pkg-config gmodule-2.0 --libs`
CFLAGS := $(INCPATH) $(LIBPATH) $(OPTIONS)
#_____________STATIC STUFF________________________________________________

#Small enough project so lets rebuild everytime
run : build-release
	./$(EXE)

run-debug : build-debug
	./$(EXE)

run-gdb : build-debug
	gdb --args ./$(EXE)

edit0 : 
	nano -c main.cpp

edit1 : 
ifeq ($(UNAME), Linux)
	kate *.cpp *.hpp &
endif
ifeq ($(UNAME), Darwin)
	@echo 'N/A'
endif

run-valgrind : build-debug
	valgrind --leak-check=yes --show-reachable=yes --tool=memcheck ./EXE

build-release : CFLAGS += -O3
build-release : $(EXE)

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE) : $(FILES)
	$(CC) $^ -o $(EXE) $(CFLAGS)

build-debug : CFLAGS += -g -DDEBUG
build-debug : $(EXE)

clean :
	rm -rf *.o* $(EXE)*

rebuild : clean build-release
