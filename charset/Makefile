# Makefile for the Sherlock Charset Library (c) 1997--2002 Martin Mares <mj@ucw.cz>

DIRS+=charset

LIBCHARSET_MODS=toupper tolower tocat toligatures unaccent charconv setnames fb-charconv
LIBCHARSET_INCLUDES=charconv.h unicat.h fb-charconv.h

obj/charset/libcharset.a: $(addsuffix .o,$(addprefix obj/charset/,$(LIBCHARSET_MODS)))
obj/charset/libcharset.so: $(addsuffix .oo,$(addprefix obj/charset/,$(LIBCHARSET_MODS)))

INCLUDES+=obj/charset/.include-stamp
obj/charset/.include-stamp: $(addprefix charset/,$(LIBCHARSET_INCLUDES))
	build/install-includes charset run/include/charset $(?F)
	touch obj/charset/.include-stamp

build_charsets:
	cd charset && sh misc/generate

clean::
	rm -f charset/misc/u-*
