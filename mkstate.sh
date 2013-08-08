#!/bin/bash
# Create state files for 1 through 64 guest cores.

if [ -z $LOG2MINCPUS ]; then
  LOG2MINCPUS=0
fi

if [ -z $LOG2MAXCPUS ]; then
  LOG2MAXCPUS=6
fi

# Ensure QSIM_PREFIX is set and set LD_LIBRARY_PATH and FF appropriately.
if [ -z $QSIM_PREFIX ]; then
  QSIM_PREFIX=/usr/local
fi
export LD_LIBRARY_PATH=$QSIM_PREFIX/lib
FF=$QSIM_PREFIX/bin/qsim-fastforwarder

if [ -z $RAMSIZE ]; then
  # 3580 is the minimum needed to run Parsec with the simsmall data set.
  RAMSIZE=1024
fi

# Truncate our log file
echo > mkstate.log

for i in `seq $LOG2MINCPUS $LOG2MAXCPUS`; do
  n=`echo 2 $i ^ p | dc`
  echo "-- running qsim-fastforwarder for $n core(s) --"
  $FF linux/bzImage $n $RAMSIZE state.$n 2>&1 >> mkstate.log

#  examples/io-test \
#    $n TRACE state.$n ../benchmarks/splash2-tar/fft.tar > state.$n.testout &
done
