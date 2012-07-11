# Tests for fb-multi.c

Name:	Read Simple
Run:	../obj/ucw/fb-multi-t r
Out:	One
	LineTwo
	LinesTh
	reeLi
	nes

Name:	Read Mingle
Run:	../obj/ucw/fb-multi-t m
Out:	Mingle

Name:	Read Insane
Run:	../obj/ucw/fb-multi-t i
Out:	Insane

Name:	Read Nested
Run:	../obj/ucw/fb-multi-t n
Out:	Nested Data
	As In
	Real Usage

Name:	Read Nested Flatten
Run:	../obj/ucw/fb-multi-t f
Out:	Nested Data
	As In
	Real Usage
