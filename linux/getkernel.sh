#!/bin/sh
# Download and patch Linux kernel
KERNEL=linux-4.1.15
KERNEL_ARC=$KERNEL.tar.xz
KERNEL_URL=https://www.kernel.org/pub/linux/kernel/v4.x/$KERNEL_ARC
UNPACKAGE="tar -xf"

INITRD=`pwd`/../initrd/initrd.cpio

# Only download the archive if we don't alreay have it.
if [ ! -e $KERNEL_ARC ]; then
  echo === DOWNLOADING ARCHIVE ===
  wget $KERNEL_URL
fi

# Remove our kernel working directory if we're about to overwrite it.
if [ -e $KERNEL ]; then
  rm -rf $KERNEL
fi

echo === UNPACKING ARCHIVE ===
$UNPACKAGE $KERNEL_ARC
mv $KERNEL linux

cp $KERNEL.qsim.config linux/.config
cd linux

echo === PATCHING ===
#sed "s#%%%QSIM_INITRD_FILE%%%#\"$INITRD\"#" < ../$KERNEL.qsim.config > .config
patch -p1 < ../$KERNEL.qsim.patch

echo === BUILDING LINUX ===
make -j4
