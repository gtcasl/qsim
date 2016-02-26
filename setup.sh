#!/bin/sh
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
aarch64_tool=$PWD/tools/gcc-linaro-5.1-2015.08-x86_64_aarch64-linux-gnu

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 ; then
    echo "AARCH64 toolchain is already installed!"
else
    echo "\nAARCH64 toolchain is not set, download from linaro website..."
    echo "Press any key to continue..."
    read inp
    mkdir -p tools
    cd tools 
    wget -c "https://releases.linaro.org/components/toolchain/binaries/latest-5.1/aarch64-linux-gnu/gcc-linaro-5.1-2015.08-x86_64_aarch64-linux-gnu.tar.xz" -O aarch64_toolchain.tar.xz
    echo "\nUncompressing the toolchain..."
    tar -xf aarch64_toolchain.tar.xz
    export PATH="$PATH:$aarch64_tool/bin"
    echo "\n\nAdd the following lines to your bashrc:\n"
    echo "${bold}export PATH=\$PATH:\$QSIM_PREFIX/gcc-linaro-5.1-2015.08-x86_64_aarch64-linux-gnu/bin${normal}"
    echo "Press any key to continue..."
    read inp
    cd ..
fi

# set the QSIM environment variable
echo "Setting QSIM environment variable..."
export QSIM_PREFIX=`pwd`
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$QSIM_PREFIX/lib
echo "\n\nAdd the following lines to your bashrc:\n"
echo "${bold}export QSIM_PREFIX=$QSIM_PREFIX${bold}"
echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:\$QSIM_PREFIX/lib${normal}\n"
echo "Press any key to continue..."
read inp

echo "\n\nDown QEMU OS images? This will take a while. (Y/n):"
read inp

# Install dependencies
echo "Installing dependencies..."
echo "sudo apt-get -y build-dep qemu"
sudo apt-get -y build-dep qemu
#sudo apt-get -y install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

if [ "$inp" = "y" -o "$inp" = "Y" ]; then
  cd ..
  # get qemu images
  echo "\nDownloading arm QEMU images..."
  mkdir -p images
  cd images
  wget -c https://www.dropbox.com/s/2jplu61410tfime/arm64_images.tar.xz?dl=0 -O arm64_images.tar.xz
  wget -c https://www.dropbox.com/s/4ut7e4d5ygty020/x86_64_images.tar.xz?dl=0 -O x86_64_images.tar.xz

  echo "\nUncompresssing images. This might take a while..."
  tar -xf arm64_images.tar.xz
  tar -xf x86_64_images.tar.xz
  cd $QSIM_PREFIX
fi

# update submodules
git submodule update --init
echo "Building capstone disassembler..."
cd capstone
make -j4
echo "Building distorm disassembler..."
cd ../distorm/distorm64/build/linux
make clib 2> /dev/null
cd $QSIM_PREFIX

# build linux kernel and initrd
echo "Building Linux kernel..."
cd linux
./getkernel.sh
./getkernel.sh arm64
cd $QSIM_PREFIX

# build qemu
echo "\nConfiguring and building qemu...\n"
./build-qemu.sh release

# build qsim
# copy header files to include directory
make release install

echo "\n\nBuilding busybox"
cd initrd/
./getbusybox.sh
./getbusybox.sh arm64
cd $QSIM_PREFIX

# run tests
make tests
# run simple example
# echo "Running the cache simulator example..."
# cd qsim/arm-examples/
# make && ./cachesim

if [ $? -eq "0" ]; then
  echo "\n${bold}Setup finished successfully!${normal}"
fi
