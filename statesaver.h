/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#ifndef __STATESAVER_H
#define __STATESAVER_H

#include <qsim.h>

namespace Qsim {
  void save_state(Qsim::OSDomain &osd, const char *filename);
};

#endif
