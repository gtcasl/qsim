#!/bin/bash

if [ -n "${QSIM_PREFIX+1}" ]; then
  echo "QSIM_PREFIX environment variable is set to: ${QSIM_PREFIX}"
else
  echo "QSIM_PREFIX is not defined. Please set it to the root qsim folder."
  exit 0;
fi

if [[ $1 = "debug" ]]; then
  echo "make debug"
  debug_flags="--enable-debug --enable-debug-tcg --enable-debug-info"
  build_dir=.dbg_build
fi

if [[ $1 = "release" ]]; then
  echo "make release"
  build_dir=.opt_build
fi

echo "make $QSIM_PREFIX/lib/"
mkdir -p $QSIM_PREFIX/lib/
if [ ! -d "$build_dir" ]; then
  mkdir -p $build_dir
  cd $build_dir
  echo "make QEMU FLAGS"
  QEMU_CFLAGS="-I${QSIM_PREFIX} -g -fPIC -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wno-deprecated-declarations -Wstrict-prototypes -Wredundant-decls -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -fno-strict-aliasing -fno-common -Wendif-labels -Wmissing-include-dirs -Wempty-body -Wnested-externs -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-all -Wno-uninitialized" ../qemu/configure --extra-ldflags=-shared --target-list=aarch64-softmmu,x86_64-softmmu --disable-pie --disable-brlapi --disable-rdma --disable-rbd --disable-tcmalloc --disable-xen --disable-gtk --disable-glusterfs --disable-xfsctl --disable-uuid $debug_flags --datadir=$QSIM_PREFIX/qemu/pc-bios --with-confsuffix=/
else
  echo "enter $build_dir"
  cd $build_dir
fi

echo "build QEMU"
make -j4
echo "go back"
cd ..
echo "clean up"
rm -f build
ln -s $build_dir build
