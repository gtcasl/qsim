#!/bin/sh
# Download and patch Linux kernel
KERNEL=linux-2.6.34
KERNEL_ARC=$KERNEL.tar.bz2
KERNEL_URL=http://kernelorg.mirrors.tds.net/pub/linux/kernel/v2.6/$KERNEL_ARC
UNPACKAGE="tar -xjf"

INITRD=`pwd`/../initrd/initrd.cpio

echo === DOWNLOADING ARCHIVE ===
wget $KERNEL_URL

echo === UNPACKING ARCHIVE ===
$UNPACKAGE $KERNEL_ARC

cd $KERNEL

echo === PATCHING ===
sed "s#%%%QSIM_INITRD_FILE%%%#\"$INITRD\"#" < ../$KERNEL.qsim.config > .config
patch -p1 < ../$KERNEL.qsim.patch

cd ..
