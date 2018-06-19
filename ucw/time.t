# Tests for time-conf

Name:	Parser normal
Run:	../obj/ucw/time-conf-t 'ftp://example.com/other'
In:	123
	0.23
	1e2
	3k
	1d
Out:	123000
	230
	100000
	3000000
	86400000

Name:	Parser underflow
Run:	../obj/ucw/time-conf-t 'ftp://example.com/other'
In:	0.0001
Out:	Non-zero time rounds down to zero milliseconds

Name:	Parser overflow
Run:	../obj/ucw/time-conf-t 'ftp://example.com/other'
In:	1e30
Out:	Time too large
