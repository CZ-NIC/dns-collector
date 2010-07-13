# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=shxml
PROGS+=$(o)/shxml/xml-test

LIBSHXML_MODS=common source parse dtd
LIBSHXML_INCLUDES=xml.h dtd.h

LIBSHXML_MOD_PATHS=$(addprefix $(o)/shxml/,$(LIBSHXML_MODS))

$(o)/shxml/libshxml.a: $(addsuffix .o,$(LIBSHXML_MOD_PATHS))
$(o)/shxml/libshxml.so: $(addsuffix .oo,$(LIBSHXML_MOD_PATHS))
$(o)/shxml/libshxml.pc: $(LIBCHARSET)

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

API_LIBS+=libshxml
API_INCLUDES+=$(o)/shxml/.include-stamp
$(o)/shxml/.include-stamp: $(addprefix $(s)/shxml/,$(LIBSHXML_INCLUDES))
$(o)/shxml/.include-stamp: IDST=shxml
run/lib/pkgconfig/libshxml.pc: $(o)/shxml/libshxml.pc

INSTALL_TARGETS+=install-sh-xml
install-sh-xml:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/shxml $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/shxml/,$(LIBSHXML_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/shxml
	install -m 644 run/lib/pkgconfig/libshxml.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 run/lib/libshxml.$(LS) $(DESTDIR)$(INSTALL_LIB_DIR)

.PHONY: install-sh-xml
