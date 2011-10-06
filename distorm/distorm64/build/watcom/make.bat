set watcom=c:\watcom
"%watcom%\binnt\cl386" -O2 -c -DSUPPORT_64BIT_OFFSET ../../src/x86defs.c ../../src/wstring.c ../../src/textdefs.c ../../src/prefix.c ../../src/operands.c ../../src/insts.c ../../src/instructions.c ../../src/distorm.c ../../src/decoder.c -I%watcom%\h

"%watcom%\binnt\lib386" -out:distorm.lib x86defs.obj wstring.obj textdefs.obj prefix.obj operands.obj insts.obj instructions.obj distorm.obj decoder.obj
"%watcom%\binnt\cl386" -O2 -c ../../linuxproj/main.c -I%watcom%\h
set path=%path%;%watcom%\binnt\
"%watcom%\binnt\wlink" FILE main.obj LIBRARY distorm.lib NAME disasm.exe OPTION STACK=512K
del *.obj;*.lib;*.map
