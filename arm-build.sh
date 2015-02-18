#!/bin/bash

QEMU_CFLAGS="-fPIC -I${QSIM_PREFIX} -shared -m64 -g" ../qemu/configure --target-list=arm-softmmu --disable-pie --enable-debug
make -j8
