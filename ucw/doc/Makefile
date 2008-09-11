# Makefile for the UCW documentation, (c) 2008 Michal Vaner <vorner@ucw.cz>

DIRS+=ucw/doc

UCW_DOCS=fastbuf index basecode
UCW_INDEX=$(o)/ucw/doc/def_index.html

$(UCW_INDEX): DOC_HEAD=$(s)/ucw/doc/def_index.txt
$(UCW_INDEX): DOC_LIST=$(patsubst %,$(o)/ucw/doc/%.deflist,$(UCW_DOCS))

DOCS+=$(addprefix $(o)/ucw/doc/,$(addsuffix .html,$(UCW_DOCS)))
DOC_INDICES+=$(UCW_INDEX)
