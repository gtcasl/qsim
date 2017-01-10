#!/bin/bash

if [ -n "${QSIM_PREFIX+1}" ]; then
  echo "QSIM_PREFIX environment variable is set to: ${QSIM_PREFIX}"
else
  echo "QSIM_PREFIX is not defined. Please set it to the root qsim folder."
  exit 0;
fi

if [[ $1 = "debug" ]]; then
  debug_flags="--enable-debug --enable-debug-tcg --enable-debug-info"
  build_dir=.dbg_build
fi

if [[ $1 = "release" ]]; then
  build_dir=.opt_build
fi

mkdir -p $QSIM_PREFIX/lib/
if [ ! -d "$build_dir" ]; then
  mkdir -p $build_dir
  cd $build_dir
  QEMU_CFLAGS="-I${QSIM_PREFIX} -g -fPIC -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 \
  -D_LARGEFILE_SOURCE -Wstrict-prototypes -Wredundant-decls -Wall -Wundef     \
  -Wwrite-strings -Wmissing-prototypes -fno-strict-aliasing -fno-common       \
  -Wendif-labels -Wmissing-include-dirs -Wempty-body -Wnested-externs         \
  -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers             \
  -Wtype-limits -fstack-protector-all -Wno-uninitialized" ../qemu/configure   \
             --extra-ldflags=-shared --disable-libssh2 --disable-tcmalloc \
             --disable-seccomp --disable-{bzip2,snappy,lzo} --disable-usb-redir \
             --disable-libusb --disable-libnfs  \
             --disable-libiscsi --disable-rbd  --disable-spice --disable-attr \
             --disable-cap-ng --disable-linux-aio --disable-uuid --disable-brlapi \
             --disable-vnc-{jpeg,sasl,png} --disable-rdma --disable-bluez \
             --disable-curl --disable-curses --disable-sdl \
             --disable-gtk  --disable-tpm --disable-vte --disable-vnc  \
             --disable-xen --disable-opengl --target-list=x86_64-softmmu,aarch64-softmmu \
             --disable-glusterfs --disable-xfsctl $debug_flags
else
  cd $build_dir
fi

make -j4
cd ..
rm -f build
ln -s $build_dir build
