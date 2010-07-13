# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=sherlock/xml
PROGS+=$(o)/sherlock/xml/xml-test

LIBSHXML_MODS=common source parse dtd
LIBSHXML_INCLUDES=xml.h dtd.h

LIBSHXML_MOD_PATHS=$(addprefix $(o)/sherlock/xml/,$(LIBSHXML_MODS))

$(o)/sherlock/xml/libshxml.a: $(addsuffix .o,$(LIBSHXML_MOD_PATHS))
$(o)/sherlock/xml/libshxml.so: $(addsuffix .oo,$(LIBSHXML_MOD_PATHS))
$(o)/sherlock/xml/libshxml.pc: $(LIBSH) $(LIBCHARSET)

$(o)/sherlock/xml/common.o: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/common.oo: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/source.o: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/source.oo: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/dtd.o: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/dtd.oo: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/parse.o: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/parse.oo: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/unicat.h: $(s)/sherlock/xml/unicat.pl
	$(M)GEN $(addprefix $(o)/sherlock/xml/unicat,.h .c)
	$(Q)$< $(addprefix $(o)/sherlock/xml/unicat,.h .c)
	$(Q)touch $@

TESTS+=$(o)/sherlock/xml/xml-test.test
$(o)/sherlock/xml/xml-test: $(o)/sherlock/xml/xml-test.o $(LIBSHXML)
$(o)/sherlock/xml/xml-test.test: $(o)/sherlock/xml/xml-test

API_LIBS+=libshxml
API_INCLUDES+=$(o)/sherlock/xml/.include-stamp
$(o)/sherlock/xml/.include-stamp: $(addprefix $(s)/sherlock/xml/,$(LIBSHXML_INCLUDES))
$(o)/sherlock/xml/.include-stamp: IDST=sherlock/xml
run/lib/pkgconfig/libshxml.pc: $(o)/sherlock/xml/libshxml.pc

INSTALL_TARGETS+=install-sh-xml
install-sh-xml:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/sherlock/xml $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/sherlock/xml/,$(LIBSHXML_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/sherlock/xml
	install -m 644 run/lib/pkgconfig/libshxml.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 run/lib/libshxml.$(LS) $(DESTDIR)$(INSTALL_LIB_DIR)

.PHONY: install-sh-xml
