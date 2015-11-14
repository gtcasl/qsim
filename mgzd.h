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
#include <string.h>

static const char* TMP_DIR = "/tmp/";
static const char* TMP_PFX = "qsim_XXXXXX";

namespace Mgzd {
  struct lib_t {
    void* handle;
    std::string file;
  };

  static lib_t __attribute__((unused)) open(const char *libfile) {
    lib_t lib;

    // Use $QSIM_TMP, if it's set.
    const char* tmpdir = getenv("QSIM_TMP");
    if (tmpdir) TMP_DIR = tmpdir;

    const int buf_size = 1024; // 1KB
    char tmpfile[buf_size], buf[buf_size];

    size_t size = sizeof(TMP_DIR);
    strncpy(tmpfile, TMP_DIR, size);
    strcat(tmpfile, TMP_PFX);

    int fd = mkstemp(tmpfile);
    FILE* fp = fdopen(fd, "wb");
    FILE* libfp = fopen(libfile, "r");

    if (!libfp) {
      std::cerr << "Cannot open library " << libfile << std::endl;
      exit(1);
    }

    while ((size = fread(buf, 1, buf_size, libfp)) > 0) {
      if (fwrite(buf, 1, size, fp) != size) {
        std::cerr << "couldn't write whole buffer" << std::endl;
        exit(1);
      }
    }

    // Make temporary copy of libfile, so opening multiple copies of the same
    // file results in independent copies of global variables.
    lib.file = tmpfile;
    std::cout << "Opening " << lib.file.c_str() << std::endl;

    lib.handle = dlopen(lib.file.c_str(), RTLD_NOW|RTLD_LOCAL);
    if (lib.handle == NULL) {
      std::cerr << "dlopen(\"" << lib.file.c_str() << "\") failed:  " 
                << dlerror() << '\n';
    }

    return lib;
  }

  static void __attribute__((unused)) close(const lib_t &lib) {
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
