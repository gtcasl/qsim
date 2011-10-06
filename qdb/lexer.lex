/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
%{
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "util.h"
#include "tok.h"

#define YY_NO_YYWRAP
inline int yywrap(void) { return 0; }

size_t linebuf_pos = 0;
size_t linebuf_len = 0;
char *linebuf = NULL;

/*We redefine YY_INPUT to use readline. Only goes one character at a time.*/
#define YY_INPUT(buf, result, max_size) { \
  if (linebuf == NULL) { \
    linebuf = readline("qdb> "); \
    if (linebuf == NULL) linebuf = strdup("quit"); \
    add_history(linebuf); \
    linebuf_len = strlen(linebuf); \
    if (linebuf_len == 0) { free(linebuf); linebuf = NULL; } \
    linebuf_pos = 0; \
  } \
  if (max_size == 0) { \
    result = 0; \
  } else if (linebuf_pos == linebuf_len) { \
    free(linebuf); linebuf = NULL; \
    buf[0] = '\n'; buf[1] = '\0'; \
    result = 1; \
  } else if (max_size > linebuf_len-linebuf_pos + 1) {	\
    strncpy(buf, linebuf+linebuf_pos, linebuf_len-linebuf_pos+1);\
    result = linebuf_len-linebuf_pos+1; \
    linebuf_pos = linebuf_len;  \
    strncat(buf, "\n", 1); \
    free(linebuf); linebuf = NULL; \
  } else { \
    buf[0] = linebuf[linebuf_pos++]; buf[1] = '\0'; \
    result = 1; \
  } \
}

int yyerror(char* s);
%}

%start NONCOMMAND FILETIME FAIL

space [ \t]*
endl  \r?\n
dblquot \"

%%
<INITIAL>[Ii][A-Za-z]*     {BEGIN NONCOMMAND; return T_INTERVAL;}
<INITIAL>[Ss][Yy][A-Za-z]* {BEGIN NONCOMMAND; return T_SYNC;    }
<INITIAL>[Tt][Rr][A-Za-z]* {BEGIN NONCOMMAND; return T_TRACE;   }
<INITIAL>[Mm][A-Za-z]*     {BEGIN NONCOMMAND; return T_MEMTR;   }
<INITIAL>[Rr][Uu][A-Za-z]* {BEGIN NONCOMMAND; return T_RUN;     }
<INITIAL>[Tt][Ii][A-Za-z]* {BEGIN NONCOMMAND; return T_TICK;    }
<INITIAL>[Dd][Uu][A-Za-z]* {BEGIN NONCOMMAND; return T_DUMP;    }
<INITIAL>[Dd][Ii][A-Za-z]* {BEGIN NONCOMMAND; return T_DISAS;   }
<INITIAL>[Ll][A-Za-z]*     {BEGIN FILETIME;   return T_LSYMS;   }
<INITIAL>[Uu][A-Za-z]*     {BEGIN NONCOMMAND; return T_USYMS;   }
<INITIAL>[Cc][A-Za-z]*     {BEGIN NONCOMMAND; return T_CPUSTAT; }
<INITIAL>[Ss][Ee][A-Za-z]* {BEGIN NONCOMMAND; return T_SET;     }
<INITIAL>[Ss][Tt][A-Za-z]* {BEGIN NONCOMMAND; return T_STEP;    }
<INITIAL>[Pp][A-Za-z]*     {BEGIN NONCOMMAND; return T_PROF;    }
<INITIAL>[Rr][Ee][A-Za-z]* {BEGIN NONCOMMAND; return T_REPORT;  }
<INITIAL>[Ww][A-Za-z]*     {BEGIN NONCOMMAND; return T_WATCH;   }
<INITIAL>[Bb][A-Za-z]*     {BEGIN NONCOMMAND; return T_BREAK;   }
<INITIAL>[Qq][A-Za-z]*     {BEGIN NONCOMMAND; return T_QUIT;    }

<INITIAL>[Hh][A-Za-z]*     {return T_HELP;    }

<NONCOMMAND>[Oo][Nn]       {return T_ON;      }
<NONCOMMAND>[Oo][Ff][Ff]   {return T_OFF;     }

<NONCOMMAND>%[A-Za-z0-9]+ {
  yylval.i = register_lookup(yytext);
  return T_REG;     
}

<NONCOMMAND>(0|[1-9][0-9]*) {
  unsigned long long num;
  sscanf(yytext, "%llu", &num);
  yylval.l = num;
  return T_LITERAL;
}

<NONCOMMAND>0[0-7]+ {
  uint64_t num;
  sscanf(yytext, "%llo", &num);
  yylval.l = num;
  return T_LITERAL; 
}

<NONCOMMAND>0x[0-9a-fA-F]+ {
  uint64_t num;
  sscanf(yytext, "%llx", &num);
  yylval.l = num;
  return T_LITERAL; 
}

<NONCOMMAND>[A-Za-z_][A-Za-z0-9_]* {
  yylval.l = symbol_lookup(yytext);
  return T_LITERAL; 
}

<FILETIME>[^ \t\n]+ {
  yylval.s = strdup(yytext);
  BEGIN NONCOMMAND;
  return T_FILE;
}

<FAIL>{endl} {BEGIN INITIAL; return T_ERROR; }
<FAIL>. {/*Ignore everything up to the next endline while in fail state.*/}

{space} {}
{endl}  {BEGIN INITIAL; return T_END;}
.       { printf("Unrecognized \"%s\".\n", yytext); BEGIN FAIL;}
