/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#ifndef __UTIL_H
#define __UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

uint64_t symbol_lookup  (const char* symbol );
int      register_lookup(const char* regname);

#ifdef __cplusplus
};
#endif

#endif
