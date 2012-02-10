# Create state files for 1 through 1024 guest cores.

# For now, we're making these really tiny to avoid filling up the disk. On the
# other hand, we will probably run out of space for kernel stacks. This works
# out to an amusingly tiny (L1 cache sized) 64kB per core for the 1024 core
# case.
RAMSIZE=64

# Truncate our log file
echo > mkstate.log

for i in `seq 0 10`; do
  n=`echo 2 $i ^ p | dc`
  echo "-- running qsim-fastforwarder for $n core(s) --"
  qsim-fastforwarder linux/bzImage $n $RAMSIZE state.$n 2>&1 >> mkstate.log

  examples/io-test \
    $n TRACE state.$n ../benchmarks/splash2-tar/fft.tar > state.$n.testout &
done
