#!/bin/bash

QSIM_PREFIX=${QSIM_PREFIX:-"/usr/local"}
LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-"$QSIM_PREFIX/lib"}
LOGFILE=${LOGFILE:-"test_qcache.log"}
BENCHMARK_DIR=${BENCHMARK_DIR:-"$QSIM_PREFIX/benchmarks"}

export QSIM_PREFIX
export LD_LIBRARY_PATH

TARFILES=$BENCHMARK_DIR/*-tar/*.tar
THREADCOUNTS="1 2 4"
GUESTCORECOUNT=64

echo > $LOGFILE

for TAR in $TARFILES; do
  APP=`echo $TAR | sed 's/\.tar//' | sed 's/^.*\///'`
  echo === $APP ===
  for i in $THREADCOUNTS; do
    echo -n "$i "
    ./main ../state.$GUESTCORECOUNT $TAR $i TRACE.$APP.$i >> $LOGFILE
  done
  echo
done