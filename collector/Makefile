.PHONY: all clean build-deps clean-deps help docs

# TODO: add -flto, -fuse-ld later

PROTOC_C=protoc-c
PROTOC=protoc
CC=gcc
WARNS=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros
ifeq ($(CC),clang)
WARNS+=-Wno-gnu-statement-expression -Wno-language-extension-token
endif
CFLAGS=-O1 -g3 -std=gnu99 -pedantic $(WARNS) -Ilibs/libucw/run/include/ #-flto
# Using local static libucw
LDLIBS=libs/libucw/run/lib/libucw-6.5.a -lpcap -llz4 -lprotobuf-c -lpthread
LDFLAGS=


PROG=dnscol
PROTO=dnsquery
DEPS=$(wildcard *.h) $(PROTO).pb-c.h
SRCS=$(wildcard *.c) $(PROTO).pb-c.c
OBJS=$(sort $(SRCS:.c=.o))

all: $(PROG) protodump.py

docs:
	doxygen Doxyfile

clean:
	rm -f $(OBJS) $(PROG) $(PROTO)_pb2.py $(PROTO).pb-c.c $(PROTO).pb-c.h *.pyc
	rm -rf __pycache__/
	rm -rf docs/html

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(PROTO).pb-c.c $(PROTO).pb-c.h: $(PROTO).proto
	$(PROTOC_C) --c_out=. $<

$(PROTO)_pb2.py: $(PROTO).proto
	$(PROTOC) --python_out=. $<

protodump.py: $(PROTO)_pb2.py

help:
	@echo "Valid make targets:"
	@echo "  all ($(PROG), protodump.py), clean"
	@echo "  build-deps, clean-deps"


### Dependencies

build-deps: build-deps-libucw

clean-deps: clean-deps-libucw

.PHONY: build-deps-libucw build-deps-liblz4 build-deps-protobuf-c clean-deps-libucw clean-deps-liblz4 clean-deps-protobuf-c

## LibUCW

build-deps-libucw: libs/libucw/run/lib/libucw-6.5.a
libs/libucw/run/lib/libucw-6.5.a:
	git submodule init
	git submodule update
	cd libs/libucw && \
	    ./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG CONFIG_LOCAL -CONFIG_SHARED -CONFIG_DOC -CONFIG_CHARSET && \
	    make all

clean-deps-libucw:
	cd libs/libucw && \
	    make clean

## LibLZ4

build-deps-liblz4: libs/lz4/lib/liblz4.a
libs/lz4/lib/liblz4.a:
	git submodule init
	git submodule update
	cd libs/lz4 && \
	    make lib

clean-deps-liblz4:
	cd libs/lz4 && \
	    make clean

## Protobuf-c

build-deps-protobuf-c: libs/protobuf-c/protoc-c/protoc-c
libs/protobuf-c/protoc-c/protoc-c:
	git submodule init
	git submodule update
	cd libs/protobuf-c && \
	    ./autogen.sh && \
	    ./configure && \
	    make

clean-deps-protobuf-c:
	cd libs/protobuf-c && \
	    make clean
