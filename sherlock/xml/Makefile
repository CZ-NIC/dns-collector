# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=sherlock/xml

LIBSHXML_MODS=xml
LIBSHXML_INCLUDES=xml.h dtd.h

LIBSHXML_MOD_PATHS=$(addprefix $(o)/sherlock/xml/,$(LIBSHXML_MODS))

$(o)/sherlock/xml/libshxml.a: $(addsuffix .o,$(LIBSHXML_MOD_PATHS))
$(o)/sherlock/xml/libshxml.so: $(addsuffix .oo,$(LIBSHXML_MOD_PATHS))
$(o)/sherlock/xml/libshxml.pc: $(LIBUCW) $(LIBCHARSET)

$(o)/sherlock/xml/xml-t: $(LIBSHXML)
$(o)/sherlock/xml/xml.o: $(o)/sherlock/xml/unicat.h
$(o)/sherlock/xml/unicat.h: $(s)/sherlock/xml/unicat.pl
	$(M)GEN $@
	$(Q)$< >$@

API_INCLUDES+=$(o)/sherlock/xml/.include-stamp
$(o)/sherlock/xml/.include-stamp: $(addprefix $(s)/sherlock/xml/,$(LIBSHXML_INCLUDES))
$(o)/sherlock/xml/.include-stamp: IDST=sherlock/xml
