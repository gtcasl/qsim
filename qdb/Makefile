###############################################################################
# Qemu Simulation Framework (qsim)                                            #
# Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     #
# a C++ API, for the use of computer architecture researchers.                #
#                                                                             #
# This work is licensed under the terms of the GNU GPL, version 2. See the    #
# COPYING file in the top-level directory.                                    #
###############################################################################
CXXFLAGS=-g -pthread -I../distorm/
LDFLAGS=-pthread -lreadline -ltermcap -ldl -lqsim
OBJS = qdb.o lex.yy.o parser.tab.o help.o ../distorm/distorm64.a

#Killed making docs by default because the output is annoying/Doxygen isn't
#really configured yet/Doxygen shouldn't need to run every time anyway.
all: qdb

qdb: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS)

parser.tab.cpp tok.h: parser.ypp
	bison -v --defines=tok.h parser.ypp

lex.yy.c: tok.h lexer.lex
	flex -olex.yy.c lexer.lex

help.o:       help.cpp       tok.h
qdb.o:        qdb.cpp        util.h qdb.h banner.h
lex.yy.o:     lex.yy.c       util.h tok.h
parser.tab.o: parser.tab.cpp qdb.h

clean:
	rm -rf doc/* qdb *.tab.cpp lex.yy.c *.o \#*\# *~ tok.h