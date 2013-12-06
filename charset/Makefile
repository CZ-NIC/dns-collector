# Makefile for the UCW Charset Library (c) 1997--2007 Martin Mares <mj@ucw.cz>

DIRS+=charset

LIBCHARSET_MODS=toupper tolower tocat toligatures unaccent charconv setnames fb-charconv stk-charconv mp-charconv
LIBCHARSET_INCLUDES=charconv.h unicat.h fb-charconv.h stk-charconv.h mp-charconv.h

$(o)/charset/libucw-charset.a: $(addsuffix .o,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libucw-charset-pic.a: $(addsuffix .oo,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libucw-charset.so: $(addsuffix .oo,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libucw-charset.pc: $(LIBUCW)

ifdef CONFIG_STATIC_PIC
$(o)/charset/libucw-charset.pc: $(o)/charset/libucw-charset-pic.a
endif

API_LIBS+=libucw-charset
API_INCLUDES+=$(o)/charset/.include-stamp
$(o)/charset/.include-stamp: $(addprefix $(s)/charset/,$(LIBCHARSET_INCLUDES))
$(o)/charset/.include-stamp: IDST=charset
run/lib/pkgconfig/libucw-charset.pc: $(o)/charset/libucw-charset.pc

ifdef CONFIG_CHARSET_UTILS
PROGS+=$(o)/charset/cs2cs
endif

$(o)/charset/cs2cs: $(o)/charset/cs2cs.o $(LIBUCW) $(o)/charset/libucw-charset.pc

build_charsets:
	cd $(s)/charset && sh misc/generate

clean::
	rm -f $(s)/charset/misc/u-*

INSTALL_TARGETS+=install-libucw-charset
install-libucw-charset:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/charset $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/charset/,$(LIBCHARSET_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/charset
	install -m 644 run/lib/libucw-charset.$(LS) $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/pkgconfig/libucw-charset.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
ifdef CONFIG_STATIC_PIC
	install -m 644 run/lib/libucw-charset-pic.$(LS) $(DESTDIR)$(INSTALL_LIB_DIR)
endif

.PHONY: install-libucw-charset
