# Create state files for 1 through 1024 guest cores.

# This is the minimum needed to run Parsec with the simsmall data set.
RAMSIZE=3072

# Truncate our log file
echo > mkstate.log

for i in `seq 0 6`; do
  n=`echo 2 $i ^ p | dc`
  echo "-- running qsim-fastforwarder for $n core(s) --"
  qsim-fastforwarder linux/bzImage $n $RAMSIZE state.$n 2>&1 >> mkstate.log

  examples/io-test \
    $n TRACE state.$n ../benchmarks/splash2-tar/fft.tar > state.$n.testout &
done
