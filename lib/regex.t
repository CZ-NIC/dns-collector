# Tests for the regex module

Run:	obj/lib/regex-t 'a.*b.*c'
In:	abc
	ajkhkbbbbbc
	Aabc
Out:	MATCH
	MATCH
	NO MATCH

Run:	obj/lib/regex-t -i 'a.*b.*c'
In:	aBc
	ajkhkbBBBBC
	Aabc
Out:	MATCH
	MATCH
	MATCH

Run:	obj/lib/regex-t -i '(ahoj|nebo)'
In:	Ahoj
	nEBo
	ahoja
	(ahoj|nebo)
Out:	MATCH
	MATCH
	NO MATCH
	NO MATCH

Run:	obj/lib/regex-t '\(ahoj\)'
In:	(ahoj)
	ahoj
Out:	MATCH
	NO MATCH

Run:	obj/lib/regex-t '(.*b)*'
In:	ababababab
	ababababababababababababababababababababababababababababa
Out:	MATCH
	NO MATCH

Run:	obj/lib/regex-t '(.*)((aabb)|cc)(b.*)' '\1<\3>\4'
In:	aaabbb
	aabbccb
	abcabc
	aaccbb
Out:	a<aabb>b
	aabb<>b
	NO MATCH
	aa<>bb

Run:	obj/lib/regex-t '.*\?(.*&)*([a-z_]*sess[a-z_]*|random|sid|S_ID|rnd|timestamp|referer)=.*'
In:	/nemecky/ubytovani/hotel.php?sort=&cislo=26&mena=EUR&typ=Hotel&luz1=ANO&luz2=ANO&luz3=&luz4=&luz5=&maxp1=99999&maxp2=99999&maxp3=99999&maxp4=99999&maxp5=99999&apart=&rada=8,9,10,11,19,22,26,27,28,29,3&cislo=26&mena=EUR&typ=Hotel&luz1=ANO&luz2=ANO&luz3=&luz4=&luz5=&maxp1=99999&maxp2=99999&maxp3=99999&maxp4=99999&maxp5=99999&apart=&rada=8,9,10,11,19,22,26,27,28,29,3&cislo=26&mena=EUR&typ=Hotel&luz1=ANO&luz2=ANO&luz3=&luz4=&luz5=&maxp1=99999&maxp2=99999&maxp3=99999&maxp4=99999&maxp5=99999&apart=&rada=8,9,10,11,19,22,26,27,28,29,3
Out:	NO MATCH
