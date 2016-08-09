.PHONY: all clean build-deps clean-deps help docs

CC=gcc # or clang
WARNS=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros

ifeq ($(CC),clang)
WARNS+=-Wno-gnu-statement-expression -Wno-language-extension-token
endif

CFLAGS=-O3 -g3 -std=gnu11 -pedantic $(WARNS) -I../libs/libucw/run/include/ -I../libs/lz4/lib/ -rdynamic -pthread # ADD -flto ?

# Using local static libucw and liblz4
LDLIBS=../libs/libucw/run/lib/libucw-6.5.a -ltrace -lpthread -lknot
LDFLAGS=

PROG=dnscol
DEPS=$(wildcard *.h)
SRCS=common.c input.c frame_queue.c packet_frame.c worker_frame_logger.c main.c \
     output.c output_csv.c packet.c worker_packet_matcher.c packet_hash.c
OBJS=$(sort $(SRCS:.c=.o))

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG) $(PROTO)_pb2.py $(PROTO).pb-c.c $(PROTO).pb-c.h *.pyc
	rm -rf __pycache__/

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)


### Protobufs - optional for now

$(PROTO).pb-c.c $(PROTO).pb-c.h: $(PROTO).proto
	$(PROTOC_C) --c_out=. $<

PROTO=dnsquery
PROTOC_C=protoc-c
PROTOC=protoc
$(PROTO)_pb2.py: $(PROTO).proto
	$(PROTOC) --python_out=. $<

protodump.py: $(PROTO)_pb2.py

