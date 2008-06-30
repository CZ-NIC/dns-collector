# Tests of the SHA1 module

Run:	echo -n "abc" | ../obj/lib/sha1-t
Out:	a9993e364706816aba3e25717850c26c9cd0d89d

Run:	echo -n "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" | ../obj/lib/sha1-t
Out:	84983e441c3bd26ebaae4aa1f95129e5e54670f1
