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
	abababababa
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
