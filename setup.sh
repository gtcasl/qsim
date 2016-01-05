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

# Install dependencies
echo "sudo apt-get -y build-dep qemu"
sudo apt-get -y build-dep qemu
sudo apt-get -y install gcc-aarch64-linux-gnu

# set the QSIM environment variable
echo "Setting QSIM environment variable..."
export QSIM_PREFIX=`pwd`
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$QSIM_PREFIX/lib
echo "\n\nAdd the following lines to your bashrc:\n"
echo "${bold}export QSIM_PREFIX=$QSIM_PREFIX${bold}"
echo "export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:\$QSIM_PREFIX/lib${normal}\n"
echo "Press any key to continue..."

read inp

# build qemu
echo "\nConfiguring and building qemu...\n"
./build-qemu.sh release

# build qsim
# copy header files to include directory
make release install

echo "\n\nDown QEMU OS images? This will take a while. (Y/n):"
read inp
if [ ! "$inp" != "y" ]; then
  # get qemu images
  echo "\nDownloading arm QEMU images..."
  wget -c https://www.dropbox.com/s/2jplu61410tfime/arm64_images.tar.xz?dl=0 -O arm64_images.tar.xz
  wget -c https://www.dropbox.com/s/4ut7e4d5ygty020/x86_64_images.tar.xz?dl=0 -O x86_64_images.tar.xz

  echo "\nUncompresssing images. This might take a while..."
  tar -xf arm64_images.tar.xz
  tar -xf x86_64_images.tar.xz
fi

echo "\n\nBuilding Linux kernel"
cd linux
./getkernel.sh $ARCH

echo "\n\nBuilding busybox"
cd ../initrd/
./getbusybox.sh $ARCH

# run simple example
# echo "Running the cache simulator example..."
# cd qsim/arm-examples/
# make && ./cachesim
