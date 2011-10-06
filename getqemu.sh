#!/bin/sh
# Download and patch QEMU

QEMU=qemu-0.12.3
QEMU_ARCHIVE=$QEMU.tar.gz
QEMU_URL=http://download.savannah.gnu.org/releases/qemu/$QEMU_ARCHIVE
QEMU_PATCH=$QEMU.qsim.patch

UNPACK="tar -xzf"

echo === DOWNLOADING ARCHIVE ===
#wget $QEMU_URL

echo === UNPACKING ARCHIVE ===
$UNPACK $QEMU_ARCHIVE

cd $QEMU

echo === PATCHING ===
patch -p1 < ../$QEMU_PATCH

echo === RUNNING CONFIG SCRIPT ===
CFLAGS="-fPIC -I`cd .. && pwd` -shared -m64 -g" ./configure --disable-sdl \
  --audio-drv-list= --audio-card-list= --disable-brlapi --disable-curses  \
  --disable-bluez --disable-kvm --disable-nptl --disable-linux-aio        \
  --disable-blobs --target-list=x86_64-softmmu

cd ..
