# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=ucw-xml
PROGS+=$(o)/ucw-xml/xml-test

LIBXML_MODS=common source parse dtd ns
LIBXML_MOD_PATHS=$(addprefix $(o)/ucw-xml/,$(LIBXML_MODS))
LIBXML_INCLUDES=xml.h dtd.h
LIBXML_DEPS=$(LIBUCW) $(LIBCHARSET)

$(o)/ucw-xml/libucw-xml$(LV).a: $(addsuffix .o,$(LIBXML_MOD_PATHS))
$(o)/ucw-xml/libucw-xml$(LV).so: $(addsuffix .oo,$(LIBXML_MOD_PATHS)) $(LIBXML_DEPS)
$(o)/ucw-xml/libucw-xml$(LV).so: SONAME_SUFFIX=.0
$(o)/ucw-xml/libucw-xml.pc: $(LIBXML_DEPS)

ifdef CONFIG_INSTALL_API
$(o)/ucw-xml/libucw-xml.pc: $(addprefix $(o)/ucw-xml/libucw-xml$(LV),.a .so)
endif

$(addsuffix .o,$(LIBXML_MOD_PATHS)): $(o)/ucw-xml/unicat.h
$(addsuffix .oo,$(LIBXML_MOD_PATHS)): $(o)/ucw-xml/unicat.h
$(o)/ucw-xml/unicat.h: $(s)/ucw-xml/unicat.pl
	$(M)GEN $(addprefix $(o)/ucw-xml/unicat,.h .c)
	$(Q)$< $(addprefix $(o)/ucw-xml/unicat,.h .c)
	$(Q)touch $@

TESTS+=$(o)/ucw-xml/xml-test.test
$(o)/ucw-xml/xml-test: $(o)/ucw-xml/xml-test.o $(LIBXML) $(LIBCHARSET) $(LIBUCW)
$(o)/ucw-xml/xml-test.test: $(o)/ucw-xml/xml-test

API_LIBS+=libucw-xml
API_INCLUDES+=$(o)/ucw-xml/.include-stamp
$(o)/ucw-xml/.include-stamp: $(addprefix $(s)/ucw-xml/,$(LIBXML_INCLUDES))
$(o)/ucw-xml/.include-stamp: IDST=ucw-xml
run/lib/pkgconfig/libucw-xml.pc: $(o)/ucw-xml/libucw-xml.pc

INSTALL_TARGETS+=install-libucw-xml-lib
install-libucw-xml-lib:
	install -d -m 755 $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw-xml$(LV).so.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-xml$(LV).so.0.0
	ln -sf libucw-xml$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-xml$(LV).so.0
.PHONY: install-libucw-xml-lib

INSTALL_TARGETS+=install-libucw-xml-api
install-libucw-xml-api:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/xml $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 run/lib/pkgconfig/libucw-xml.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/ucw-xml/,$(LIBXML_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/xml
	ln -sf libucw-xml$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-xml$(LV).so
	install -m 644 run/lib/libucw-xml$(LV).a $(DESTDIR)$(INSTALL_LIB_DIR)
.PHONY: install-libucw-xml-api
