.PHONY: all clean veryclean docs libucw install prog

all: prog
veryclean:: clean

LDLIBS?=
LDFLAGS?=
CFLAGS?=-O2 -g -pedantic

## Options

# USE_TCMALLOC=1

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

LDLIBS+= $(LIBUCW_LIBRARY)
CFLAGS+= -I$(LIBUCW_DIR)/run/include/

## tinycbor

TINYCBOR_DIR?=./tinycbor
TINYCBOR_LIBRARY?=$(TINYCBOR_DIR)/lib/libtinycbor-freestanding.a

tinycbor: $(TINYCBOR_LIBRARY)

$(TINYCBOR_LIBRARY):
	cd $(TINYCBOR_DIR)/ && make lib/libtinycbor-freestanding.a

veryclean::
	cd $(TINYCBOR_DIR)/ && make clean

LDLIBS+= $(TINYCBOR_LIBRARY)
CFLAGS+= -I$(TINYCBOR_DIR)/src/

## dns-collector

WARNS?=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros

ifeq ($(CC),clang)
WARNS+=-Wno-gnu-statement-expression -Wno-language-extension-token
endif

CFLAGS+= $(WARNS) -rdynamic -pthread -std=gnu11
LDLIBS+= -lknot -ltrace -lpthread

ifdef USE_TCMALLOC
    CFLAGS+= -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    LDLIBS+= -ltcmalloc
endif

PROG=./dns-collector

prog: $(PROG)

include src/Makefile

## install

PREFIX?=/usr/local
DESTDIR?=$(PREFIX)
CONFIG=./dns-collector.conf

install: $(PROG)
	install -m 755 -D $(PROG) $(DESTDIR)/usr/bin/dns-collector
	install -m 644 -D $(CONFIG) $(DESTDIR)/etc/dns-collector/dns-collector.conf
	
