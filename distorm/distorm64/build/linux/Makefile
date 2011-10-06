#
# diStorm64 (Linux Port)
#

TARGET	= libdistorm64.so
COBJS	= ../../src/x86defs.o ../../src/wstring.o ../../src/textdefs.o ../../src/prefix.o ../../src/operands.o ../../src/insts.o ../../src/instructions.o ../../src/distorm.o ../../src/decoder.o
PYOBJS	= ../../src/x86defs.o ../../src/wstring.o ../../src/textdefs.o ../../src/pydistorm.o ../../src/prefix.o ../../src/operands.o ../../src/insts.o ../../src/instructions.o ../../src/decoder.o
CC	= gcc
CFLAGS	= -O2 -Wall -fPIC -DSUPPORT_64BIT_OFFSET -D_DLL

all: clib py

clean:
	/bin/rm -rf ../../src/*.o ${TARGET} ../../distorm64.a

clib: ${COBJS}
	${CC} ${CFLAGS} ${VERSION} ${COBJS} -fPIC -shared -o ${TARGET}
	ar rs ../../distorm64.a ${COBJS}

py: ${PYOBJS}
	${CC} ${CFLAGS} ${VERSION} ${PYOBJS} -fPIC -shared -o ${TARGET}

install: libdistorm64.so
	install -s ${TARGET} /usr/local/lib
	@echo "... running ldconfig might be smart ..."

.c.o:
	${CC} ${CFLAGS} ${VERSION} -c $< -o $@

