# Tests of the MD5 module

Name:	MD5-1
Run:	echo -n "abc" | ../obj/ucw/md5-t
Out:	900150983cd24fb0d6963f7d28e17f72

Name:	MD5-2
Run:	echo -n "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" | ../obj/ucw/md5-t
Out:	8215ef0796a20bcaaae116d3876c664a
