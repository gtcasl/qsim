#!/bin/bash
# Download and configure busybox.

# NOTE: This is optional. The binary of busybox distributed with QSim should
# work perfectly adequately.

BBOX=busybox-1.24.1
BBOX_ARCHIVE=$BBOX.tar.bz2
BBOX_URL=http://www.busybox.net/downloads/$BBOX_ARCHIVE

UNPACK="tar -xjf"

pushd ../linux
LINUX_DIR=`pwd`/linux/
popd

# Download the archive if we don't already have it.
if [ ! -e $BBOX_ARCHIVE ]; then
  echo === DOWNLOADING ARCHIVE ===
  wget $BBOX_URL
fi

# Delete the busybox directory if it already exists.
if [ -e $BBOX ]; then
  rm -r $BBOX
fi

echo === UNPACKING ARCHIVE ===
$UNPACK $BBOX_ARCHIVE

echo === COPYING CONFIG ===
sed "s#\\%LINUX_DIR\\%#$LINUX_DIR#g" < busybox-config \
  > $BBOX/.config
