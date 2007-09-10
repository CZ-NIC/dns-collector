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
WIDTHS="6 7 8 9 10 11 12 13 14"

# Which sort-test tests we try
TESTS="2,5,8,15"

# Check various bit widths of the radix sorter
rm -f tmp/radix-*
for W in $WIDTHS ; do
	log "Compiling with $W-bit radix splits"
	rm -f $BUILD/obj/lib/sorter/sort-test{,.o}
	( cd $BUILD && make CEXTRA="-DFORCE_RADIX_BITS=$W" obj/lib/sorter/sort-test )
	log "Running the tests"
	$BUILD/obj/lib/sorter/sort-test -s$SIZE -t$TESTS -v 2>&1 | tee tmp/radix-$W
done

log "Trying with radix-sort switched off"
$BUILD/obj/lib/sorter/sort-test -s$SIZE -t$TESTS -v -d32 2>&1 | tee tmp/radix-0

FILES=""
for W in 0 $WIDTHS ; do
	a=tmp/radix-$W
	echo >$a.out "$W bits"
	sed 's/.* \([0-9.]\+\)s internal sorting.*/\1/;t;d' <$a >>$a.out
	FILES="$FILES $a.out"
done

log "These are the results:"
echo "test#,$TESTS" | tr , '\n' >tmp/radix-tests
paste tmp/radix-tests $FILES
