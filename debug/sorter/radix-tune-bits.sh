#!/bin/bash
# An utility for tuning the Sherlock's radix sorter
# (c) 2007 Martin Mares <mj@ucw.cz>
set -e
UCW_PROGNAME="$0"
. lib/libucw.sh

# Path to Sherlock build directory
[ -n "$BUILD" ] || BUILD=..
[ -f "$BUILD/lib/sorter/sorter.h" ] || die "BUILD does not point to Sherlock build directory"

# Find out sort buffer size
parse-config 'Sorter{##SortBuffer}'
SORTBUF=$CF_Sorter_SortBuffer
[ "$SORTBUF" -gt 0 ] || die "Unable to determine SortBuffer"
log "Detected sort buffer size $SORTBUF"

# Size of the test -- should be slightly less than a half of SortBuffer
SIZE=$(($SORTBUF/2 - 8192))
log "Decided to benchmark sorting of $SIZE byte data"

# Which bit widths we try
WIDTHS="0 6 7 8 9 10 11 12 13 14"

# Which RadixThresholds we try
THRS="2000 4000 10000 20000 50000"

# Which sort-test tests we try
TESTS="2,5,8,15"

# Check various bit widths of the radix sorter
rm -f tmp/radix-*
for W in $WIDTHS ; do
	rm -f $BUILD/obj/lib/sorter/sort-test{,.o}
	if [ $W = 0 ] ; then
		log "Compiling with no radix splits"
		( cd $BUILD && make obj/lib/sorter/sort-test )
		OPT="-d32"
	else
		log "Compiling with $W-bit radix splits"
		( cd $BUILD && make CEXTRA="-DFORCE_RADIX_BITS=$W" obj/lib/sorter/sort-test )
		OPT=
	fi
	for THR in $THRS ; do
		log "Testing with RadixThreshold=$THR"
		$BUILD/obj/lib/sorter/sort-test -SThreads.DefaultStackSize=2M -SSorter.RadixThreshold=$THR -s$SIZE -t$TESTS $OPT -v 2>&1 | tee -a tmp/radix-$W
	done
done

echo "thresh" >tmp/radix-thrs
echo "test#" >tmp/radix-tests
for THR in $THRS ; do
	for TEST in `echo $TESTS | tr ',' ' '` ; do
		echo $THR >>tmp/radix-thrs
		echo $TEST >>tmp/radix-tests
	done
done

FILES="tmp/radix-thrs tmp/radix-tests"
for W in $WIDTHS ; do
	a=tmp/radix-$W
	echo >$a.out "$W bits"
	sed 's/.* \([0-9.]\+\)s internal sorting.*/\1/;t;d' <$a >>$a.out
	FILES="$FILES $a.out"
done

log "These are the results:"
paste $FILES
