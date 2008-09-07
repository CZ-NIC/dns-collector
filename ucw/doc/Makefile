# Makefile for the UCW documentation, (c) 2008 Michal Vaner <vorner@ucw.cz>

DIRS+=ucw/doc

UCW_DOCS=fastbuf
UCW_INDEX=$(o)/ucw/doc/index.html

$(UCW_INDEX): DOC_HEAD:=$(s)/ucw/doc/index.txt
$(UCW_INDEX): DOC_LIST:=$(patsubst %,$(o)/ucw/doc/%.deflist,$(UCW_DOCS))

DOCS+=$(addprefix $(o)/ucw/doc/,$(addsuffix .html,$(UCW_DOCS)))
DOC_INDECES+=$(UCW_INDEX)
