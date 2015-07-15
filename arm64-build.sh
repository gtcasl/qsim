#!/bin/bash

if [ -n "${QSIM_PREFIX+1}" ]; then
  echo "QSIM_PREFIX environment variable is set to: ${QSIM_PREFIX}"
else
  echo "QSIM_PREFIX is not defined. Please set it to the root qsim folder."
  exit 0;
fi

if [ -z $1 ]; then
  debug_flags="--enable-debug --enable-debug-tcg --enable-debug-info"
fi

QEMU_CFLAGS="-I${QSIM_PREFIX} -shared -m64 -g  -Werror -fPIC -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wstrict-prototypes -Wredundant-decls -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -fno-strict-aliasing -fno-common -Wendif-labels -Wmissing-include-dirs -Wempty-body -Wnested-externs -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wold-style-declaration -Wold-style-definition -Wtype-limits -fstack-protector-all -Wno-uninitialized" ../qemu/configure --target-list=aarch64-softmmu --disable-pie $debug_flags
make -j16 && cp aarch64-softmmu/qemu-system-aarch64 $QSIM_PREFIX/lib/libqemu-qsim.so
