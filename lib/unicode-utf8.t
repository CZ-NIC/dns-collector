# Tests for the Unicode UTF-8 module

Run:	obj/lib/unicode-utf8-t put
In:	0041 0048 004f 004a
Out:	41 48 4f 4a

Run:	obj/lib/unicode-utf8-t put
In:	00aa 01aa 02a5 05a5 0a5a 15a5 2a5a 5a5a a5a5
Out:	c2 aa c6 aa ca a5 d6 a5 e0 a9 9a e1 96 a5 e2 a9 9a e5 a9 9a ea 96 a5

Run:	obj/lib/unicode-utf8-t get
In:	41 48 4f 4a
Out:	0041 0048 004f 004a

Run:	obj/lib/unicode-utf8-t get
In:	c2 aa c6 aa ca a5 d6 a5 e0 a9 9a e1 96 a5 e2 a9 9a e5 a9 9a ea 96 a5
Out:	00aa 01aa 02a5 05a5 0a5a 15a5 2a5a 5a5a a5a5

Run:	obj/lib/unicode-utf8-t get
In:	84 ff f9 f8 c2 aa 41
Out:	fffc fffc fffc fffc 00aa 0041

Run:	obj/lib/unicode-utf8-t put32
In:	15a5a 2a5a5 5a5a5 a5a5a 15a5a5 2a5a5a 5a5a5a a5a5a5 15a5a5a 2a5a5a5 5a5a5a5 a5a5a5a 15a5a5a5 2a5a5a5a 5a5a5a5a
Out:	f0 95 a9 9a f0 aa 96 a5 f1 9a 96 a5 f2 a5 a9 9a f5 9a 96 a5 f8 8a a5 a9 9a f8 96 a5 a9 9a f8 a9 9a 96 a5 f9 96 a5 a9 9a fa a9 9a 96 a5 fc 85 a9 9a 96 a5 fc 8a 96 a5 a9 9a fc 95 a9 9a 96 a5 fc aa 96 a5 a9 9a fd 9a 96 a5 a9 9a

Run:	obj/lib/unicode-utf8-t get32
In:	f0 95 a9 9a f0 aa 96 a5 f1 9a 96 a5 f2 a5 a9 9a f5 9a 96 a5 f8 8a a5 a9 9a f8 96 a5 a9 9a f8 a9 9a 96 a5 f9 96 a5 a9 9a fa a9 9a 96 a5 fc 85 a9 9a 96 a5 fc 8a 96 a5 a9 9a fc 95 a9 9a 96 a5 fc aa 96 a5 a9 9a fd 9a 96 a5 a9 9a
Out:	15a5a 2a5a5 5a5a5 a5a5a 15a5a5 2a5a5a 5a5a5a a5a5a5 15a5a5a 2a5a5a5 5a5a5a5 a5a5a5a 15a5a5a5 2a5a5a5a 5a5a5a5a

Run:	obj/lib/unicode-utf8-t get32
In:	fe 83 81
Out:	fffc fffc fffc
