# Makefile for the UCW-XML documentation

DIRS+=ucw-xml/doc

XML_DOCS=xml index
XML_DOCS_HTML=$(addprefix $(o)/ucw-xml/doc/,$(addsuffix .html,$(XML_DOCS)))

DOCS+=$(XML_DOCS_HTML)
DOC_MODULES+=ucw-xml
$(XML_DOCS_HTML): DOC_MODULE=ucw-xml

ifdef CONFIG_DOC
INSTALL_TARGETS+=install-libucw-xml-docs
endif

.PHONY: install-libucw-xml-docs

install-libucw-xml-docs: $(XML_DOCS_HTML)
	install -d -m 755 $(DESTDIR)$(INSTALL_DOC_DIR)/ucw-xml/
	install -m 644 $^ $(DESTDIR)$(INSTALL_DOC_DIR)/ucw-xml/
