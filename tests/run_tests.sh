#!/bin/bash

set -e -u

DATA="akuma fail crash"
if [ ! -f data/akuma.*.pcap.bz2 ]; then
    echo "You need to decrypt and decompress the test data first"
    exit 1
fi

rm out -rf
mkdir out -p

for D in $DATA; do
    for C in confs/*.conf; do
        F=data/$D*.pcap*
        OF="$D-${C##*/}.out"
        CMD="../dns-collector -C $C $F -o out/$OF"
        echo "Running: $CMD"

        $CMD || exit 1

        case $C in ( *gzip* )
            if [ -f out/$OF ]; then 
                mv out/$OF out/$OF.gz
                gzip -d out/$OF.gz
            fi
        esac
        if [ -f "data/$OF" ]; then
            diff "out/$OF" "data/$OF" || exit 1
            echo "diff: out/$OF and data/$OF match"
        fi
    done
done
echo "All done"
