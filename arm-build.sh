#!/bin/bash

QEMU_CFLAGS="-fPIC -I/home/pranith/devops/code/qsim -shared -m64 -g" ../qemu/configure --target-list=arm-softmmu --disable-pie --enable-debug
make -j8
