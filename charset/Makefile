# Makefile for the Sherlock Charset Library (c) 1997--2002 Martin Mares <mj@ucw.cz>

DIRS+=charset

LIBCHARSET_MODS=toupper tolower tocat toligatures unaccent charconv setnames fb-charconv

obj/charset/libcharset.a: $(addsuffix .o,$(addprefix obj/charset/,$(LIBCHARSET_MODS)))
obj/charset/libcharset.so: $(addsuffix .oo,$(addprefix obj/charset/,$(LIBCHARSET_MODS)))

build_charsets:
	cd charset && sh misc/generate

clean::
	rm -f charset/misc/u-*
