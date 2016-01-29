#!/usr/bin/env python

import sys
import dnsquery_pb2

t = sys.stdin.read()

while t:
    l = ord(t[0]) + (ord(t[1]) << 8)
    q = dnsquery_pb2.DnsQuery()
    q.ParseFromString(t[2:2+l])
    print("\n" + str(q))
    t = t[2+l:]

