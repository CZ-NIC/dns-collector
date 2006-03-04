# Makefile for the Sherlock Charset Library (c) 1997--2002 Martin Mares <mj@ucw.cz>

DIRS+=charset

LIBCHARSET_MODS=toupper tolower tocat toligatures unaccent charconv setnames fb-charconv stk-charconv mp-charconv
LIBCHARSET_INCLUDES=charconv.h unicat.h fb-charconv.h stk-charconv.h mp-charconv.h

$(o)/charset/libcharset.a: $(addsuffix .o,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libcharset.so: $(addsuffix .oo,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))

INCLUDES+=$(o)/charset/.include-stamp
$(o)/charset/.include-stamp: $(addprefix $(s)/charset/,$(LIBCHARSET_INCLUDES))
	$(s)/build/install-includes $(s)/charset run/include/charset $(?F)
	touch $(o)/charset/.include-stamp

build_charsets:
	cd $(s)/charset && sh misc/generate

clean::
	rm -f $(s)/charset/misc/u-*
