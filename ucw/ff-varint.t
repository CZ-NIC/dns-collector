# Tests for the FF-Varint module

Name:	bput_varint
Run:	../obj/ucw/ff-varint-t bput_varint
In:	deadbeef 2222 feeddeadbabebeef
Out:	f0 ce 8d 7e 6f a1 a2 ff fd eb da a5 aa 9e 7e 6f

Name:   bget_varint
Run:    ../obj/ucw/ff-varint-t bget_varint
In:     ff fd eb da a5 aa 9e 7e 6f f0 ce 8d 7e 6f a1 a2
Out:    feeddeadbabebeef deadbeef 2222
