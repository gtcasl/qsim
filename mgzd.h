#ifndef __MGZD_H
#define __MGZD_H
/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static const char* TMP_DIR = "/tmp";
static const char* TMP_PFX = "qsim_tmp";

namespace Mgzd {
  struct lib_t {
    void* handle;
    std::string file;
  };

  static lib_t open(const char *libfile) {
    lib_t lib;

    // Use $QSIM_TMP, if it's set.
    const char* tmpdir = getenv("QSIM_TMP");
    if (tmpdir) TMP_DIR = tmpdir;

    // Make temporary copy of libfile, so opening multiple copies of the same
    // file results in independent copies of global variables.
    const char* tmp_filename_ptr = tempnam(TMP_DIR, TMP_PFX);
    lib.file = tmp_filename_ptr;
    free((void *)tmp_filename_ptr);

    std::ostringstream cp_command;

    cp_command << "cp " << libfile << ' ' << lib.file;
    int r;
    if ((r = system(cp_command.str().c_str())) != 0) {
      std::cerr << "system(\"" << cp_command.str() 
                << "\") returned " << r <<".\nrm /";
      exit(1);
    }
    std::cout << "Opening " << lib.file.c_str() << std::endl;

    lib.handle = dlopen(lib.file.c_str(), RTLD_NOW|RTLD_LOCAL);
    if (lib.handle == NULL) {
      std::cerr << "dlopen(\"" << lib.file.c_str() << "\") failed:  " 
                << dlerror() << '\n';
    }

    return lib;
  }

  static void close(const lib_t &lib) {
    //dlclose(lib.handle);
    unlink(lib.file.c_str());
  }

  template <typename T> static void sym(T *&ret, 
					const lib_t lib, 
					const char *sym) {
    (void*&)ret = dlsym(lib.handle, sym);
    if (char *err = dlerror()) {
      std::cerr << "dlsym(\"" << lib.handle << "\", \"" << sym 
                << "\") failed:  " << err << '\n';
      exit(1);
    }
  }

};
#endif
