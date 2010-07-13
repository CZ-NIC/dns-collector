#!/bin/bash
# An utility for tuning the Sherlock's radix sorter threshold
# (c) 2007 Martin Mares <mj@ucw.cz>
set -e
UCW_PROGNAME="$0"
. lib/libucw.sh

# Path to Sherlock build directory
[ -n "$BUILD" ] || BUILD=..
[ -f "$BUILD/ucw/sorter/sorter.h" ] || die "BUILD does not point to Sherlock build directory"

# Find out sort buffer size
parse-config 'Sorter{##SortBuffer}'
SORTBUF=$CF_Sorter_SortBuffer
[ "$SORTBUF" -gt 0 ] || die "Unable to determine SortBuffer"
log "Detected sort buffer size $SORTBUF"

# Find out radix-sorter width
[ -f "$BUILD/obj/config.mk" ] || die "Sherlock source not configured"
WIDTH=`sed <$BUILD/obj/config.mk 's/^CONFIG_UCW_RADIX_SORTER_BITS=\(.*\)/\1/;t;d'`
[ -n "$WIDTH" ] || die "CONFIG_UCW_RADIX_SORTER_BITS not set (!?)"
log "Detected radix-sorter width $WIDTH"

# Maximum size of the test -- should be slightly less than a half of SortBuffer
SIZE=$(($SORTBUF/2 - 8192))

# Which sort-test test we try
TEST="2"

# Which thresholds we try
THRS="16"
T=$SIZE
while [ $T -gt 100 ] ; do
	THRS="$THRS $T"
	T=$(($T/2))
done

if true ; then

rm -f tmp/radix-*
echo "sizes" >tmp/radix-sizes
while [ $SIZE -gt 262144 ] ; do
	echo $SIZE >>tmp/radix-sizes
	for T in $THRS ; do
		log "Trying size $SIZE with threshold $T"
		$BUILD/obj/ucw/sorter/sort-test -SSorter.RadixThreshold=$T -s$SIZE -t$TEST -v 2>&1 | tee -a tmp/radix-$T
	done
	SIZE=$(($SIZE/2))
done

fi

FILES=tmp/radix-sizes
for T in $THRS ; do
	a=tmp/radix-$T
	echo >$a.out $T
	sed 's/.* \([0-9.]\+\)s internal sorting.*/\1/;t;d' <$a >>$a.out
	FILES="$FILES $a.out"
done

log "These are the results:"
paste $FILES
