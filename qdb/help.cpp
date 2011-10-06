/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/

#include <cstdio>
#include <stdint.h>
#include "tok.h"

const char* default_msg =
  "Type 'help <command>' for more info on <command>, e.g.:\n"
  "  help TR\n"
  "for usage, etc. for the 'TRACE' command.\n"
  "Commands:\n"
  "  Interval  Set interval between timer interrupts.\n"
  "  SYnc      Set number of barriers per timer interrupt.\n"
  "  TRace     Enable/disable per-instruction trace output.\n"
  "  Memtr     Enable/disable per-memory-operation trace output.\n"
  "  RUn       Run one or all CPUs for a given number of instructions.\n"
  "  TIck      Send an interrupt to a single CPU.\n"
  "  DUmp      Dump RAM contents in hex.\n"
  "  DIsas     Disassemble RAM contents.\n"
  "  Lsyms     Load symbols from file.\n"
  "  Usyms     Unload previously loaded symbols.\n"
  "  Cpustat   Display status of CPU (registers, task ID, etc.)\n"
  "  SEt       Set CPU register or memory location value.\n"
  "  STep      Singlestep one or all CPUs.\n"
  "  Prof      Enable/disable profiling of a given process.\n"
  "  REport    Print profiling report.\n"
  "  Watch     Set a watchpoint on a virtual address or symbol.\n"
  "  Break     Set a breakpoing on a virtual address or symbol.\n"
  "  Quit      Exit QDB.\n"
  ;

const char* interval_msg =
  "I[NTERVAL] <dyn-insts> [cpu]\n"
  "  dyn-insts Number of dynamic instructions before next timer interrupt.\n"
  "  cpu       CPU for which interval is being set. If omitted, interval is\n"
  "            set for all processors.\n"
  ;

const char* sync_msg =
  "SY[NC] <n>\n"
  "  n Number of global synchronization barriers per timer interrupt.\n"
  ;

const char* trace_msg =
  "TR[ACE] [tid] <toggle>\n"
  "  tid    Task ID to be traced. Omit to trace all tasks.\n"
  "  toggle \"on\" or \"off\" to enable or disable tracing, respectively.\n"
  ;

const char* memtr_msg =
  "M[EMTR] [tid] <toggle>\n"
  "  tid    Task ID to be traced. Omit to trace all tasks.\n"
  "  toggle \"on\" or \"off\" to enable or disable tracing, respectively.\n"
  ;

const char* run_msg =
  "RU[N] [dyn-insts [cpu]]\n"
  "  dyn-insts Number of dynamic instructions. Omit to run indefinitely.\n"
  "  cpu       CPU to run. Omit to run all CPUs.\n"
  ;

const char* tick_msg =
  "TI[CK] [vec [cpu]]\n"
  "  vec Interrupt vector. Defaults to 0xef(timer).\n"
  "  cpu CPU to interrupt. Omit to interrupt all CPUs.\n"
  ;

const char* dump_msg =
  "DU[MP] <paddr-start> <size>\n"
  "  paddr-start Starting physical address.\n"
  "  size        Number of bytes.\n"
  ;

const char* disas_msg =
  "DI[SAS] <paddr-start> <size>\n"
  "  paddr-start Starting physical address.\n"
  "  size        Number of bytes.\n"
  ;

const char* lsyms_msg =
  "L[SYMS] <filename> [cr3]\n"
  "  filename File from which to load symbols.\n"
  "  cr3      Page dir. for user mode symbols. Omit to load kernel symbols.\n"
  ;

const char* usyms_msg =
  "U[SYMS] [tid]\n"
  "  tid Task ID for user mode symbols. Omit to unload kernel symbols.\n"
  ;
const char* cpustat_msg =
  "C[PUSTAT] [cpu]\n"
  "  cpu ID of CPU to examine. Omit to print status of all CPUs.\n"
  ;

const char* set_msg =
  "SE[T] <cpu> %<register> <val>\n"
  "  cpu      ID of CPU.\n"
  "  register Register to be set.\n"
  "  val      Value to place in register.\n"
  "SE[T] <paddr> <val>\n"
  "  paddr    Physical RAM address.\n"
  "  val      Value to place in RAM at paddr.\n"
  ;

const char* step_msg =
  "ST[EP] [cpu]\n"
  "  cpu ID of CPU to step. Omit to step all CPUs.\n"
  ;

const char* prof_msg =
  "P[ROF] [tid] <toggle>\n"
  "  tid    Task ID to profile. Omit to profile all tasks.\n"
  "  toggle \"On\" or \"Off\" to enable and disable profiling, respectively.\n"
  ;

const char* report_msg =
  "RE[PORT]\n"
  "  (No options)\n"
  ;

const char* watch_msg =
  "W[ATCH] <vaddr> [tid]\n"
  "  vaddr Virtual address (or symbol) on which to set watchpoint.\n"
  "  tid   TID for which this watchpoint should be active.\n"
  ;

const char* break_msg =
  "B[REAK] <vaddr> [cr3]\n"
  "  vaddr Virtual address (or symbol) on which to set breakpoint.\n"
  "  cr3   Page dir. for which this breakpoint should be active.\n"
  ;

const char* quit_msg =
  "Q[UIT]\n"
  "  (No options)\n"
  ;

void show_help(int i) {
  const char* hlp_msg;

  switch (i) {
  case T_INTERVAL: hlp_msg = interval_msg; break;
  case T_SYNC:     hlp_msg = sync_msg;     break;
  case T_TRACE:    hlp_msg = trace_msg;    break;
  case T_MEMTR:    hlp_msg = memtr_msg;    break;
  case T_RUN:      hlp_msg = run_msg;      break;
  case T_TICK:     hlp_msg = tick_msg;     break;
  case T_DUMP:     hlp_msg = dump_msg;     break;
  case T_DISAS:    hlp_msg = disas_msg;    break;
  case T_LSYMS:    hlp_msg = lsyms_msg;    break;
  case T_USYMS:    hlp_msg = usyms_msg;    break;
  case T_CPUSTAT:  hlp_msg = cpustat_msg;  break;
  case T_SET:      hlp_msg = set_msg;      break;
  case T_STEP:     hlp_msg = step_msg;     break;
  case T_PROF:     hlp_msg = prof_msg;     break;
  case T_REPORT:   hlp_msg = report_msg;   break;
  case T_WATCH:    hlp_msg = watch_msg;    break;
  case T_BREAK:    hlp_msg = break_msg;    break;
 
  case T_QUIT:     hlp_msg = quit_msg;     break;
  default:         hlp_msg = default_msg;
  }

  puts(hlp_msg);
}
