# Makefile for the Sherlock Charset Library (c) 1997--2000 Martin Mares <mj@ucw.cz>

DIRS+=charset

UNI_OBJS=toupper.o tolower.o tocat.o utf8.o unaccent.o tosig.o strlen.o debug.o \
	charconv.o setnames.o

obj/charset/libcharset.a: $(addprefix obj/charset/,$(UNI_OBJS))

build_charsets: clean_charsets
	misc/unisplit
	misc/gentab _U_cat byte <misc/u-cat >U-cat.h
	misc/gentab _U_lower word <misc/u-lower >U-lower.h
	misc/gentab _U_upper word <misc/u-upper >U-upper.h
	misc/mkunacc >misc/u-unacc
	misc/gentab _U_unaccent word <misc/u-unacc >U-unacc.h
	misc/mksig >misc/u-sig
	misc/gentab _U_sig byte <misc/u-sig >U-sig.h
	misc/tabgen <misc/chartable.in >chartable.h

clean_charsets:
	rm -f misc/u-* U-*.h chartable.h
