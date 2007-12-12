# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=sherlock/xml
PROGS+=$(o)/sherlock/xml/xml-test

LIBSHXML_MODS=common parse dtd
LIBSHXML_INCLUDES=xml.h dtd.h

LIBSHXML_MOD_PATHS=$(addprefix $(o)/sherlock/xml/,$(LIBSHXML_MODS))

$(o)/sherlock/xml/libshxml.a: $(addsuffix .o,$(LIBSHXML_MOD_PATHS))
$(o)/sherlock/xml/libshxml.so: $(addsuffix .oo,$(LIBSHXML_MOD_PATHS))
$(o)/sherlock/xml/libshxml.pc: $(LIBSH) $(LIBCHARSET)

$(o)/sherlock/xml/common.o $(o)/sherlock/xml/unicat.h: $(o)/sherlock/xml/unicat.stamp
$(o)/sherlock/xml/unicat.stamp: $(s)/sherlock/xml/unicat.pl
	$(M)GEN $(addprefix $(o)/sherlock/xml/unicat,.h .c)
	$(Q)$< $(addprefix $(o)/sherlock/xml/unicat,.h .c)
	$(Q)touch $@

$(o)/sherlock/xml/xml-test: $(o)/sherlock/xml/xml-test.o $(LIBSHXML)

API_LIBS+=libshxml
API_INCLUDES+=$(o)/sherlock/xml/.include-stamp
$(o)/sherlock/xml/.include-stamp: $(addprefix $(s)/sherlock/xml/,$(LIBSHXML_INCLUDES))
$(o)/sherlock/xml/.include-stamp: IDST=sherlock/xml
run/lib/pkgconfig/libshxml.pc: $(o)/sherlock/xml/libshxml.pc
