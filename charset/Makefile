# Makefile for the UCW Charset Library (c) 1997--2007 Martin Mares <mj@ucw.cz>

DIRS+=charset

LIBCHARSET_PROGS=
LIBCHARSET_MODS=toupper tolower tocat toligatures unaccent charconv setnames fb-charconv stk-charconv mp-charconv
LIBCHARSET_INCLUDES=charconv.h unicat.h fb-charconv.h stk-charconv.h mp-charconv.h

$(o)/charset/libucw-charset.a: $(addsuffix .o,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libucw-charset-pic.a: $(addsuffix .oo,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libucw-charset.so: $(addsuffix .oo,$(addprefix $(o)/charset/,$(LIBCHARSET_MODS)))
$(o)/charset/libucw-charset.so: SONAME_SUFFIX=.$(UCW_ABI_MAJOR)
$(o)/charset/libucw-charset.pc: $(LIBUCW)

ifdef CONFIG_STATIC_PIC
$(o)/charset/libucw-charset.pc: $(o)/charset/libucw-charset-pic.a
endif
ifdef CONFIG_INSTALL_API
$(o)/charset/libucw-charset.pc: $(addprefix $(o)/charset/libucw-charset,.a -pic.a .so)
endif

API_LIBS+=libucw-charset
API_INCLUDES+=$(o)/charset/.include-stamp
$(o)/charset/.include-stamp: $(addprefix $(s)/charset/,$(LIBCHARSET_INCLUDES))
$(o)/charset/.include-stamp: IDST=charset
run/lib/pkgconfig/libucw-charset.pc: $(o)/charset/libucw-charset.pc

ifdef CONFIG_CHARSET_UTILS
LIBCHARSET_PROGS+=$(o)/charset/ucw-cs2cs
endif

$(o)/charset/ucw-cs2cs: $(o)/charset/ucw-cs2cs.o $(LIBCHARSET)

PROGS+=$(LIBCHARSET_PROGS)

build_charsets:
	cd $(s)/charset && sh misc/generate

clean::
	rm -f $(s)/charset/misc/u-*

INSTALL_LIB_TARGETS+=install-libucw-charset-lib
install-libucw-charset-lib:
	install -d -m 755 $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw-charset.so.$(UCW_ABI_MAJOR) $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-charset.so.$(UCW_ABI_VERSION)
.PHONY: install-libucw-charset-lib

INSTALL_API_TARGETS+=install-libucw-charset-api
install-libucw-charset-api:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/charset $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/charset/,$(LIBCHARSET_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/charset
	install -m 644 run/lib/pkgconfig/libucw-charset.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	ln -sf libucw-charset.so.$(UCW_ABI_VERSION) $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-charset.so
	install -m 644 run/lib/libucw-charset.a $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw-charset-pic.a $(DESTDIR)$(INSTALL_LIB_DIR)
.PHONY: install-libucw-charset-api

INSTALL_UTILS_TARGETS+=install-libucw-charset-utils
install-libucw-charset-utils:
	install -d -m 755 $(DESTDIR)$(INSTALL_BIN_DIR)
	install -m 755 $(LIBCHARSET_PROGS) $(DESTDIR)$(INSTALL_BIN_DIR)
.PHONY: install-libucw-charset-utils
