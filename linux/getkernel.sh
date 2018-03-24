#!/bin/bash
# Download and patch Linux kernel
KERNEL_MAJOR=linux-4.1
KERNEL_MINOR=39
KERNEL_ARC=$KERNEL_MAJOR.$KERNEL_MINOR.tar.xz
KERNEL_URL=https://www.kernel.org/pub/linux/kernel/v4.x/$KERNEL_ARC
UNPACKAGE="tar -xf"

INITRD=`pwd`/../initrd/initrd.cpio

ARCH=x86
CROSS=aarch64-linux-gnu-
HOST=`uname -m`
if [ "$HOST" == "aarch64" ]; then
  ARCH=arm64
  CROSS=
fi

# Only download the archive if we don't alreay have it.
if [ ! -e $KERNEL_ARC ]; then
  echo === DOWNLOADING ARCHIVE ===
  wget $KERNEL_URL
fi

# unpack if kernel does not exists
if [ ! -e linux ]; then
    echo === UNPACKING ARCHIVE ===
    $UNPACKAGE $KERNEL_ARC
    mv $KERNEL_MAJOR.$KERNEL_MINOR linux
    cd linux
    echo === PATCHING ===
    patch -p1 < ../$KERNEL_MAJOR.qsim.patch
    cd ..
fi

echo === BUILDING LINUX ===
if [ -z "$1" ]; then
  cp $KERNEL_MAJOR.qsim-$ARCH.config linux/.config
  cd linux
  make -j4 KCPPFLAGS="-fno-pic -Wno-pointer-sign"
else
  cp $KERNEL_MAJOR.qsim-arm64.config linux/.config
  cd linux
  make -j4 ARCH=arm64 CROSS_COMPILE=$CROSS KCPPFLAGS="-fno-pic -Wno-pointer-sign"
fi
