# Makefile for the Sherlock Charset Library (c) 1997--2001 Martin Mares <mj@ucw.cz>

DIRS+=charset

UNI_OBJS=toupper.o tolower.o tocat.o utf8.o unaccent.o strlen.o debug.o \
	charconv.o setnames.o unistream.o

obj/charset/libcharset.a: $(addprefix obj/charset/,$(UNI_OBJS))

build_charsets:
	cd charset && sh misc/generate

clean::
	rm -f charset/misc/u-*
