# Tests for the Varint module
# In format: <hexnumber>
# Out format: <space> <len> <hexnumber>
# when run as varint-t full:
# Out format: <space> <len> <hexnumber> <hexbyte0> <hexbyte1> ...

Name:	varint (1)
Run:	../obj/ucw/varint-t full
In:	41
Out:	1 1 41 41

Name:	varint (2)
Run:	../obj/ucw/varint-t full
In:	80
Out:	2 2 80 80 0

Name:	varint (3)
Run:	../obj/ucw/varint-t full
In:	ab
Out:	2 2 ab 80 2b

Name:	varint (4)
Run:	../obj/ucw/varint-t full
In:	0
Out:	1 1 0 0

Name:   varint (5)
Run:	../obj/ucw/varint-t full
In:	2222
Out:	2 2 2222 a1 a2

Name:   varint (6)
Run:	../obj/ucw/varint-t
In:	abcd
Out:	3 3 abcd

Name:   varint (7)
Run:	../obj/ucw/varint-t
In:	abcde
Out:	3 3 abcde

Name:   varint (8)
Run:	../obj/ucw/varint-t
In:	1020407f
Out:	4 4 1020407f

Name:   varint (9)
Run:	../obj/ucw/varint-t full
In:	deadbeef
Out:	5 5 deadbeef f0 ce 8d 7e 6f

Name:   varint (10)
Run:	../obj/ucw/varint-t
In:	4081020400f
Out:	6 6 4081020400f

Name:   varint (11)
Run:	../obj/ucw/varint-t
In:	20408010204070
Out:	8 8 20408010204070

Name:   varint (12)
Run:	../obj/ucw/varint-t
In:	feeddeadbabebeef
Out:	9 9 feeddeadbabebeef

Name:   varint (13)
Run:	../obj/ucw/varint-t
In:	102040810204080
Out:	9 9 102040810204080
