# Makefile for the UCW documentation, (c) 2008 Michal Vaner <vorner@ucw.cz>

DIRS+=ucw/doc

UCW_DOCS=basics log fastbuf index config configure install basecode hash docsys conf mempool eltpool mainloop generic growbuf unaligned lists chartype unicode prime binsearch heap binheap compress sort hashtable relnotes trans string time daemon signal varint opt alloc gary table xtypes
UCW_INDEX=$(o)/ucw/doc/def_index.html
UCW_DOCS_HTML=$(addprefix $(o)/ucw/doc/,$(addsuffix .html,$(UCW_DOCS)))

UCW_SRC_MANPAGES=ucw-tableprinter
UCW_MANPAGES+=$(addprefix  $(o)/ucw/doc/,$(addsuffix .1,$(UCW_SRC_MANPAGES)))

$(UCW_INDEX): DOC_HEAD=$(s)/ucw/doc/def_index.txt
$(UCW_INDEX): DOC_LIST=$(patsubst %,$(o)/ucw/doc/%.deflist,$(UCW_DOCS))
$(UCW_INDEX) $(UCW_DOCS_HTML): DOC_MODULE=ucw

DOCS+=$(UCW_DOCS_HTML)
DOC_INDICES+=$(UCW_INDEX)
DOC_MODULES+=ucw

ifdef CONFIG_DOC
INSTALL_TARGETS+=install-libucw-docs
endif

.PHONY: install-libucw-docs
install-libucw-docs: $(UCW_INDEX) $(UCW_DOCS_HTML) $(MANPAGES)
	install -d -m 755 $(DESTDIR)$(INSTALL_DOC_DIR)/ucw/
	install -m 644 $^ $(DESTDIR)$(INSTALL_DOC_DIR)/ucw/
	install -d -m 755 $(DESTDIR)$(INSTALL_MAN_DIR)/man1/
	install -m 644 $(UCW_MANPAGES) $(DESTDIR)$(INSTALL_MAN_DIR)/man1/
