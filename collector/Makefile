.PHONY: all clean deps

PROG=main
PROTO=dnsquery

CC=clang
WARNS=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros -Wno-gnu-statement-expression -Wno-language-extension-token
CFLAGS=-O1 -g3 -std=gnu99 -pedantic $(WARNS)
LDFLAGS=-lpcap `pkg-config --libs 'libprotobuf-c >= 1.0.0'` -lucw-6.5 -llz4

# TODO: add -flto, -fuse-ld later

DEPS=$(wildcard *.h) $(PROTO).pb-c.h
SRCS=$(wildcard *.c) $(PROTO).pb-c.c
OBJS=$(sort $(SRCS:.c=.o))

all: $(PROG) protodump.py

clean:
	rm -f $(OBJS) $(PROG) $(PROTO)_pb2.py $(PROTO).pb-c.c $(PROTO).pb-c.h *.pyc
	rm -rf __pycache__/

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(PROTO).pb-c.c $(PROTO).pb-c.h: $(PROTO).proto
	protoc-c --c_out=. $<

$(PROTO)_pb2.py: $(PROTO).proto
	protoc --python_out=. $<

protodump.py: $(PROTO)_pb2.py


deps: deps-libucw deps-liblz4

deps-libucw:
	cd ../libs/libucw
	git submodule init
	git submodule update
	./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG -CONFIG_SHARED -CONFIG_DOC
	make all

deps-liblz4:
	cd ../libs/liblz4
	git submodule init
	git submodule update
	make lib

