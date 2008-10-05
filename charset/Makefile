# Makefile for the Sherlock Charset Library (c) 1997--2007 Martin Mares <mj@ucw.cz>

DIRS+=charset

LIBCHARSET_MODS=toupper tolower tocat toligatures unaccent charconv setnames fb-charconv stk-charconv mp-charconv
LIBCHARSET_INCLUDES=charconv.h unicat.h fb-charconv.h stk-charconv.h mp-charconv.h

$(o)/charset/libcharset.a: $(addsuffix .o,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libcharset.so: $(addsuffix .oo,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libcharset.pc: $(LIBUCW)

API_LIBS+=libcharset
API_INCLUDES+=$(o)/charset/.include-stamp
$(o)/charset/.include-stamp: $(addprefix $(s)/charset/,$(LIBCHARSET_INCLUDES))
$(o)/charset/.include-stamp: IDST=charset
run/lib/pkgconfig/libcharset.pc: $(o)/charset/libcharset.pc

build_charsets:
	cd $(s)/charset && sh misc/generate

clean::
	rm -f $(s)/charset/misc/u-*

ifdef CONFIG_SHARED
LIBCHARSET_NAME=libcharset.so
else
LIBCHARSET_NAME=libcharset.a
endif

INSTALL_TARGETS+=install-libcharset
install-libcharset:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/charset $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/charset/,$(LIBCHARSET_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/charset
	install -m 644 run/lib/$(LIBCHARSET_NAME) $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/pkgconfig/libcharset.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)

.PHONY: install-libcharset
