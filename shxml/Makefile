# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=shxml
PROGS+=$(o)/shxml/xml-test

LIBSHXML_MODS=common source parse dtd
LIBSHXML_INCLUDES=xml.h dtd.h

LIBSHXML_MOD_PATHS=$(addprefix $(o)/shxml/,$(LIBSHXML_MODS))

$(o)/shxml/libucw-xml.a: $(addsuffix .o,$(LIBSHXML_MOD_PATHS))
$(o)/shxml/libucw-xml-pic.a: $(addsuffix .oo,$(LIBSHXML_MOD_PATHS))
$(o)/shxml/libucw-xml.so: $(addsuffix .oo,$(LIBSHXML_MOD_PATHS))
$(o)/shxml/libucw-xml.so: SONAME_SUFFIX=.$(UCW_ABI_VERSION)
$(o)/shxml/libucw-xml.pc: $(LIBCHARSET)

ifdef CONFIG_STATIC_PIC
$(o)/shxml/libucw-xml.pc: $(o)/shxml/libucw-xml-pic.a
endif
ifdef CONFIG_INSTALL_API
$(o)/shxml/libucw-xml.pc: $(o)/shxml/libucw-xml.a $(o)/shxml/libucw-xml-pic.a $(o)/shxml/libucw-xml.so
endif

$(o)/shxml/common.o: $(o)/shxml/unicat.h
$(o)/shxml/common.oo: $(o)/shxml/unicat.h
$(o)/shxml/source.o: $(o)/shxml/unicat.h
$(o)/shxml/source.oo: $(o)/shxml/unicat.h
$(o)/shxml/dtd.o: $(o)/shxml/unicat.h
$(o)/shxml/dtd.oo: $(o)/shxml/unicat.h
$(o)/shxml/parse.o: $(o)/shxml/unicat.h
$(o)/shxml/parse.oo: $(o)/shxml/unicat.h
$(o)/shxml/unicat.h: $(s)/shxml/unicat.pl
	$(M)GEN $(addprefix $(o)/shxml/unicat,.h .c)
	$(Q)$< $(addprefix $(o)/shxml/unicat,.h .c)
	$(Q)touch $@

TESTS+=$(o)/shxml/xml-test.test
$(o)/shxml/xml-test: $(o)/shxml/xml-test.o $(LIBSHXML)
$(o)/shxml/xml-test.test: $(o)/shxml/xml-test

API_LIBS+=libucw-xml
API_INCLUDES+=$(o)/shxml/.include-stamp
$(o)/shxml/.include-stamp: $(addprefix $(s)/shxml/,$(LIBSHXML_INCLUDES))
$(o)/shxml/.include-stamp: IDST=shxml
run/lib/pkgconfig/libucw-xml.pc: $(o)/shxml/libucw-xml.pc

INSTALL_TARGETS+=install-libucw-xml install-libucw-xml-api

install-libucw-xml:
	install -d -m 755 $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw-xml.so.$(UCW_ABI_VERSION) $(DESTDIR)$(INSTALL_LIB_DIR)

install-libucw-xml-api:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/shxml $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 run/lib/pkgconfig/libucw-xml.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/shxml/,$(LIBSHXML_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/shxml
	ln -sf libucw-xml.so.$(UCW_ABI_VERSION) $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-xml.so
	install -m 644 run/lib/libucw-xml.a $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw-xml-pic.a $(DESTDIR)$(INSTALL_LIB_DIR)

.PHONY: install-libucw-xml install-libucw-xml-api
