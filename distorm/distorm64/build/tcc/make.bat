set tccroot=c:\tcc\
"%tccroot%tcc\tcc.exe" "-I%tccroot%include" "-L%tccroot%lib" ../../src/x86defs.c ../../src/wstring.c ../../src/textdefs.c ../../src/prefix.c ../../src/operands.c ../../src/insts.c ../../src/instructions.c ../../src/distorm.c ../../src/decoder.c ../../linuxproj/main.c -o disasm.exe
