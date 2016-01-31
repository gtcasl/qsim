#!/bin/sh
# Download and patch Linux kernel
KERNEL=linux-4.1.17
KERNEL_ARC=$KERNEL.tar.xz
KERNEL_URL=https://www.kernel.org/pub/linux/kernel/v4.x/$KERNEL_ARC
UNPACKAGE="tar -xf"

INITRD=`pwd`/../initrd/initrd.cpio

# Only download the archive if we don't alreay have it.
if [ ! -e $KERNEL_ARC ]; then
  echo === DOWNLOADING ARCHIVE ===
  wget $KERNEL_URL
fi

# unpack if kernel does not exists
if [ ! -e linux ]; then
    echo === UNPACKING ARCHIVE ===
    $UNPACKAGE $KERNEL_ARC
    mv $KERNEL linux
    cd linux
    echo === PATCHING ===
    #sed "s#%%%QSIM_INITRD_FILE%%%#\"$INITRD\"#" < ../$KERNEL.qsim.config > .config
    patch -p1 < ../$KERNEL.qsim.patch
    cd ..
fi

echo === BUILDING LINUX ===
if [ ! -z "$1" ]; then
  cp $KERNEL.qsim-arm64.config linux/.config
  cd linux
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j4
else
  cp $KERNEL.qsim.config linux/.config
  cd linux
  make -j4
fi
