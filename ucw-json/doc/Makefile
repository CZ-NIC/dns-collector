# Makefile for the UCW-JSON documentation

DIRS+=ucw-json/doc

JSON_DOCS=json index
JSON_DOCS_HTML=$(addprefix $(o)/ucw-json/doc/,$(addsuffix .html,$(JSON_DOCS)))

DOCS+=$(JSON_DOCS_HTML)
DOC_MODULES+=ucw-json
$(JSON_DOCS_HTML): DOC_MODULE=ucw-json

ifdef CONFIG_DOC
INSTALL_TARGETS+=install-libucw-json-docs
endif

.PHONY: install-libucw-json-docs

install-libucw-json-docs: $(JSON_DOCS_HTML)
	install -d -m 755 $(DESTDIR)$(INSTALL_DOC_DIR)/ucw-json/
	install -m 644 $^ $(DESTDIR)$(INSTALL_DOC_DIR)/ucw-json/
