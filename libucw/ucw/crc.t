# Tests for crc

Name:	Default/small
Run:	../obj/ucw/crc-t 0 123456 37 3
Out:	d620f179

Name:	Default/big
Run:	../obj/ucw/crc-t 0 123456 10037 3
Out:	d620f179

Name:	Small/small
Run:	../obj/ucw/crc-t 1 123456 37 3
Out:	d620f179

Name:	Small/big
Run:	../obj/ucw/crc-t 1 123456 10037 3
Out:	d620f179

Name:	Large/small
Run:	../obj/ucw/crc-t 2 123456 37 3
Out:	d620f179

Name:	Large/big
Run:	../obj/ucw/crc-t 2 123456 10037 3
Out:	d620f179
