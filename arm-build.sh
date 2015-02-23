#!/bin/bash

if [ -n "${QSIM_PREFIX+1}" ]; then
  echo "QSIM_PREFIX environment variable is set to: ${QSIM_PREFIX}"
else
  echo "QSIM_PREFIX is not defined. Please set it to the root qsim folder."
  exit 0;
fi

QEMU_CFLAGS="-I/usr/include/pixman-1 -I${QSIM_PREFIX} -shared -m64 -g  -Werror -fPIC -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wstrict-prototypes -Wredundant-decls -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -fno-strict-aliasing -fno-common -Wendif-labels -Wmissing-include-dirs -Wempty-body -Wnested-externs -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wold-style-declaration -Wold-style-definition -Wtype-limits -fstack-protector-all -I/usr/include/p11-kit-1   -I/usr/include/p11-kit-1 -I/usr/include/libpng12   -I/usr/include/spice-server -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pixman-1 -I/usr/include/spice-1   -I/usr/include/libusb-1.0" ../qemu/configure --target-list=arm-softmmu --disable-pie --enable-debug
make -j8
