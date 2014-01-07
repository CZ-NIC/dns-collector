# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=xml
PROGS+=$(o)/xml/xml-test

LIBXML_MODS=common source parse dtd
LIBXML_MOD_PATHS=$(addprefix $(o)/xml/,$(LIBXML_MODS))
LIBXML_INCLUDES=xml.h dtd.h
LIBXML_DEPS=$(LIBUCW) $(LIBCHARSET)

$(o)/xml/libucw-xml$(LV).a: $(addsuffix .o,$(LIBXML_MOD_PATHS))
$(o)/xml/libucw-xml$(LV)-pic.a: $(addsuffix .oo,$(LIBXML_MOD_PATHS))
$(o)/xml/libucw-xml$(LV).so: $(addsuffix .oo,$(LIBXML_MOD_PATHS)) $(LIBXML_DEPS)
$(o)/xml/libucw-xml$(LV).so: SONAME_SUFFIX=.0
$(o)/xml/libucw-xml.pc: $(LIBXML_DEPS)

ifdef CONFIG_STATIC_PIC
$(o)/xml/libucw-xml.pc: $(o)/xml/libucw-xml$(LV)-pic.a
endif
ifdef CONFIG_INSTALL_API
$(o)/xml/libucw-xml.pc: $(addprefix $(o)/xml/libucw-xml$(LV),.a .so)
endif

$(o)/xml/common.o: $(o)/xml/unicat.h
$(o)/xml/common.oo: $(o)/xml/unicat.h
$(o)/xml/source.o: $(o)/xml/unicat.h
$(o)/xml/source.oo: $(o)/xml/unicat.h
$(o)/xml/dtd.o: $(o)/xml/unicat.h
$(o)/xml/dtd.oo: $(o)/xml/unicat.h
$(o)/xml/parse.o: $(o)/xml/unicat.h
$(o)/xml/parse.oo: $(o)/xml/unicat.h
$(o)/xml/unicat.h: $(s)/xml/unicat.pl
	$(M)GEN $(addprefix $(o)/xml/unicat,.h .c)
	$(Q)$< $(addprefix $(o)/xml/unicat,.h .c)
	$(Q)touch $@

TESTS+=$(o)/xml/xml-test.test
$(o)/xml/xml-test: $(o)/xml/xml-test.o $(LIBXML) $(LIBCHARSET) $(LIBUCW)
$(o)/xml/xml-test.test: $(o)/xml/xml-test

API_LIBS+=libucw-xml
API_INCLUDES+=$(o)/xml/.include-stamp
$(o)/xml/.include-stamp: $(addprefix $(s)/xml/,$(LIBXML_INCLUDES))
$(o)/xml/.include-stamp: IDST=xml
run/lib/pkgconfig/libucw-xml.pc: $(o)/xml/libucw-xml.pc

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
	install -m 644 $(addprefix run/include/xml/,$(LIBXML_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/xml
	ln -sf libucw-xml$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-xml$(LV).so
	install -m 644 run/lib/libucw-xml$(LV).a $(DESTDIR)$(INSTALL_LIB_DIR)
.PHONY: install-libucw-xml-api
