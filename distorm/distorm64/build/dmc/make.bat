set DMCPATH=C:\dm\bin
"%DMCPATH%\dmc" -c -DSUPPORT_64BIT_OFFSET ../../src/x86defs.c ../../src/wstring.c ../../src/textdefs.c ../../src/prefix.c ../../src/operands.c ../../src/insts.c ../../src/instructions.c ../../src/distorm.c ../../src/decoder.c
"%DMCPATH%\lib" -n -c distorm.lib x86defs.obj wstring.obj textdefs.obj prefix.obj operands.obj insts.obj instructions.obj distorm.obj decoder.obj
"%DMCPATH%\dmc" ../../linuxproj/main.c distorm.lib -o disasm.exe
del *.obj;*.map;*.lib
