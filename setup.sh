#!/bin/bash
#
# Run this script to set up the qsim environment for the first time.
# You can read the following steps to see what each is doing.
#
# Author: Pranith Kumar
# Date: 01/05/2016
# Usage: ./setup.sh {arm64}

bold=$(tput bold)
normal=$(tput sgr0)

ARCH=$1

# setup the aarch64 toolchain
aarch64_tool=$PWD/tools/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 ; then
    echo -e "AARCH64 toolchain is already installed!"
else
    echo -e "\nAARCH64 toolchain is not set, download from linaro website..."
    echo -e "Press any key to continue..."
    read inp
    mkdir -p tools
    cd tools 
    wget -c "https://github.com/gtcasl/qsim_prebuilt/releases/download/v0.1/aarch64_toolchain.tar.xz"
    echo -e "\nUncompressing the toolchain..."
    tar -xf aarch64_toolchain.tar.xz
    export PATH="$PATH:$aarch64_tool/bin"
    echo -e "\n\nAdd the following lines to your bashrc:\n"
    echo -e "${bold}export PATH=\$PATH:\$QSIM_PREFIX/tools/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu/bin${normal}"
    cd ..
fi

# set the QSIM environment variable
echo -e "Setting QSIM environment variable..."
export QSIM_PREFIX=`pwd`
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$QSIM_PREFIX/lib
echo -e "\n\nAdd the following lines to your bashrc:\n"
echo -e "${bold}export QSIM_PREFIX=$QSIM_PREFIX${bold}"
echo -e "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:\$QSIM_PREFIX/lib${normal}\n"
echo -e "Press any key to continue..."
read inp

echo -e "\n\nDown QEMU OS images? This will take a while. (y/N):"
read inp

if [ "$inp" = "y" -o "$inp" = "Y" ]; then
  # get qemu images
  echo -e "\nDownloading arm QEMU images..."
  mkdir -p images
  cd images
  wget -c https://github.com/gtcasl/qsim_prebuilt/releases/download/v0.1/arm64_images.tar.xz
  wget -c https://github.com/gtcasl/qsim_prebuilt/releases/download/v0.1/x86_64_images.tar.xz

  echo -e "\nUncompresssing images. This might take a while..."
  tar -xf arm64_images.tar.xz
  tar -xf x86_64_images.tar.xz
  cd $QSIM_PREFIX
fi

# update submodules
git submodule update --init
echo -e "Building capstone disassembler..."
cd capstone
make -j4
echo -e "Building distorm disassembler..."
cd ../distorm/distorm64/build/linux
make clib 2> /dev/null
cd $QSIM_PREFIX
echo -e "Get qemu submodules..."
cd qemu
git submodule update --init pixman
git submodule update --init dtc
cd $QSIM_PREFIX

# build linux kernel and initrd
echo -e "Building Linux kernel..."
cd linux
./getkernel.sh
./getkernel.sh arm64
cd $QSIM_PREFIX

# build qemu
echo -e "\nConfiguring and building qemu...\n"
./build-qemu.sh release

# build qsim
# copy header files to include directory
make release install

echo -e "\n\nBuilding busybox"
cd initrd/
./getbusybox.sh
./getbusybox.sh arm64
cd $QSIM_PREFIX

# run tests
make tests
# run simple example
# echo -e "Running the cache simulator example..."
# cd qsim/arm-examples/
# make && ./cachesim

if [ $? -eq "0" ]; then
  echo -e "\n${bold}Setup finished successfully!${normal}"
fi
