#!/usr/bin/env python3
"""
A simple debug utility to print all the CBOR-encoded items from the
standard input. Does not handle compression -- if your data is
compressed, use e.g. as:

    cat data.cbor.gz | gzip -d | ./dump_cbor_stream.py

"""

import sys
import cbor

while True:
    try:
        d = cbor.load(sys.stdin.buffer)
    except EOFError:
        break
    try:
        print(d)
    except BrokenPipeError:
        break
