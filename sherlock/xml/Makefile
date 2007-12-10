# Makefile for the XML parser
# (c) 2007 Pavel Charvat <pchar@ucw.cz>

DIRS+=sherlock/xml

LIBSH_MODS+=xml/xml
LIBSH_XML_INCLUDES=xml/xml.h

$(o)/sherlock/xml/xml-t: $(LIBSH) $(LIBCHARSET)
$(o)/sherlock/xml/xml.o: $(o)/sherlock/xml/xml-ucat.h
$(o)/sherlock/xml/xml-ucat.h: $(s)/sherlock/xml/xml-ucat.pl
	$(M)GEN $@
	$(Q)$< >$@

API_INCLUDES+=$(o)/sherlock/xml/.include-stamp
$(o)/sherlock/xml/.include-stamp: $(addprefix $(s)/sherlock/xml/,$(LIBSH_XML_INCLUDES))
$(o)/sherlock/xml/.include-stamp: IDST=sherlock/xml

include $(s)/sherlock/perl/Makefile
