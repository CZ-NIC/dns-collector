# Tests for the Unicode module

Name:	bput_utf8 ASCII
Run:	../obj/ucw/ff-unicode-t bput_utf8
In:	0041 0048 004f 004a
Out:	41 48 4f 4a

Name:	bput_utf8 BMP
In:	00aa 01aa 02a5 05a5 0a5a 15a5 2a5a 5a5a a5a5
Out:	c2 aa c6 aa ca a5 d6 a5 e0 a9 9a e1 96 a5 e2 a9 9a e5 a9 9a ea 96 a5

Name:	bget_utf8 ASCII
Run:	../obj/ucw/ff-unicode-t bget_utf8
In:	41 48 4f 4a
Out:	0041 0048 004f 004a

Name:	bget_utf8 BMP
In:	c2 aa c6 aa ca a5 d6 a5 e0 a9 9a e1 96 a5 e2 a9 9a e5 a9 9a ea 96 a5
Out:	00aa 01aa 02a5 05a5 0a5a 15a5 2a5a 5a5a a5a5

Name:	bget_utf8 garbage
In:	84 ff f9 f8 c2 aa 41
Out:	fffc fffc fffc fffc 00aa 0041

Name:	bget_utf8 denormalized
In:	c1 bf  e0 9f bf
Out:	fffc fffc

Name:	bput_utf8_32
Run:	../obj/ucw/ff-unicode-t bput_utf8_32
In:	15a5a 2a5a5 5a5a5 a5a5a 15a5a5 2a5a5a 5a5a5a a5a5a5 15a5a5a 2a5a5a5 5a5a5a5 a5a5a5a 15a5a5a5 2a5a5a5a 5a5a5a5a
Out:	f0 95 a9 9a f0 aa 96 a5 f1 9a 96 a5 f2 a5 a9 9a f5 9a 96 a5 f8 8a a5 a9 9a f8 96 a5 a9 9a f8 a9 9a 96 a5 f9 96 a5 a9 9a fa a9 9a 96 a5 fc 85 a9 9a 96 a5 fc 8a 96 a5 a9 9a fc 95 a9 9a 96 a5 fc aa 96 a5 a9 9a fd 9a 96 a5 a9 9a

Name:	bget_utf8_32
Run:	../obj/ucw/ff-unicode-t bget_utf8_32
In:	f0 95 a9 9a f0 aa 96 a5 f1 9a 96 a5 f2 a5 a9 9a f5 9a 96 a5 f8 8a a5 a9 9a f8 96 a5 a9 9a f8 a9 9a 96 a5 f9 96 a5 a9 9a fa a9 9a 96 a5 fc 85 a9 9a 96 a5 fc 8a 96 a5 a9 9a fc 95 a9 9a 96 a5 fc aa 96 a5 a9 9a fd 9a 96 a5 a9 9a
Out:	15a5a 2a5a5 5a5a5 a5a5a 15a5a5 2a5a5a 5a5a5a a5a5a5 15a5a5a 2a5a5a5 5a5a5a5 a5a5a5a 15a5a5a5 2a5a5a5a 5a5a5a5a

Name:	bget_utf8_32 garbage
In:	fe 83 81
Out:	fffc fffc fffc

Name:	bget_utf8_32 denormalized
In:	c1 bf  e0 9f bf  f0 8f bf bf  f8 87 bf bf bf  fc 83 bf bf bf
Out:	fffc fffc fffc fffc fffc

Name:   bput_utf16_be
Run:    ../obj/ucw/ff-unicode-t bput_utf16_be
In:     0041 004a 2a5f feff 0000 10ffff ffff 10000
Out:    00 41 00 4a 2a 5f fe ff 00 00 db ff df ff ff ff d8 00 dc 00

Name:   bput_utf16_le
Run:    ../obj/ucw/ff-unicode-t bput_utf16_le
In:     0041 004a 2a5f feff 0000 10ffff ffff 10000
Out:    41 00 4a 00 5f 2a ff fe 00 00 ff db ff df ff ff 00 d8 00 dc

Name:   bget_utf16_be
Run:    ../obj/ucw/ff-unicode-t bget_utf16_be
In:     00 41 00 4a 2a 5f fe ff 00 00 db ff df ff ff ff d8 00 dc 00
Out:    0041 004a 2a5f feff 0000 10ffff ffff 10000

Name:   bget_utf16_be bad surrogates
Run:    ../obj/ucw/ff-unicode-t bget_utf16_be
In:     dc 1a 2a 5f d8 01 d8 01 2a 5f d8 01
Out:    fffc 2a5f fffc 2a5f fffc

Name:   bget_utf16_le
Run:    ../obj/ucw/ff-unicode-t bget_utf16_le
In:     41 00 4a 00 5f 2a ff fe 00 00 ff db ff df ff ff 00 d8 00 dc
Out:    0041 004a 2a5f feff 0000 10ffff ffff 10000

Name:   bget_utf16_le bad surrogates
Run:    ../obj/ucw/ff-unicode-t bget_utf16_le
In:     1a dc 5f 2a 01 d8 01 d8 5f 2a 01 d8
Out:    fffc 2a5f fffc 2a5f fffc
