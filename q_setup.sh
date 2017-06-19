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
ARCH=arm64

# build qemu
#echo -e "\n\nCLEAN QEMU"
#rm -fr .dbg_build/
echo -e "\n\nBuilding QEMU"
./build-qemu.sh debug

# build qsim
# copy header files to include directory
echo -e "\n\nBuilding QSIM"
make debug install 

#echo -e "\n\nBuilding busybox"
#cd $QSIM_PREFIX
#cd initrd/
#./getbusybox.sh arm64
#cd $QSIM_PREFIX

# run tests
#make tests
# run simple example
# echo -e "Running the cache simulator example..."
# cd qsim/arm-examples/
# make && ./cachesim

if [ $? -eq "0" ]; then
  echo -e "\n${bold}Setup finished successfully!${normal}"
fi

