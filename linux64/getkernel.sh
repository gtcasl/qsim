#!/bin/sh
# Download and patch Linux kernel
KERNEL=linux-3.5.2
KERNEL_ARC=$KERNEL.tar.bz2
KERNEL_URL=http://www.kernel.org/pub/linux/kernel/v3.0/$KERNEL_ARC
UNPACKAGE="tar -xjf"

INITRD=`pwd`/../initrd64/initrd.cpio

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

cd $KERNEL

echo === LOADING CONFIG ===
sed "s#%%%QSIM_INITRD_FILE%%%#\"$INITRD\"#" < ../$KERNEL.qsim.config > .config

cd ..
