.PHONY: all clean veryclean docs libucw

PROG=./dnscol

all: $(PROG)


## Docs

clean::
	rm -rf docs/html

docs:
	doxygen Doxyfile

## libs/libUCW

LIBUCW_DIR?=./libucw
LIBUCW_LIBRARY?=$(LIBUCW_DIR)/run/lib/libucw-6.5.a

libucw: $(LIBUCW_LIBRARY)

$(LIBUCW_LIBRARY):
	cd $(LIBUCW_DIR)/ && \
	    ./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG CONFIG_LOCAL \
	                -CONFIG_SHARED -CONFIG_DOC -CONFIG_CHARSET && \
	    make runtree libs api

veryclean::
	cd $(LIBUCW_DIR)/ && make clean


## dnscol

LDLIBS?=
LDFLAGS?=
CFLAGS?=-O2 -g -pedantic
WARNS?=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros

ifeq ($(CC),clang)
WARNS+=-Wno-gnu-statement-expression -Wno-language-extension-token
endif

CFLAGS+=$(WARNS) -rdynamic -pthread -std=gnu11 -I$(LIBUCW_DIR)/run/include/
LDLIBS+=-lknot -ltrace -lpthread $(LIBUCW_DIR)/run/lib/libucw-6.5.a

ifdef USE_TCMALLOC
    CFLAGS+=-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    LDLIBS+=-ltcmalloc
endif

include src/Makefile
