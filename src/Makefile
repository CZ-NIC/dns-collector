.PHONY: all clean build-deps clean-deps help docs

CC=gcc # or clang
WARNS=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros

ifeq ($(CC),clang)
WARNS+=-Wno-gnu-statement-expression -Wno-language-extension-token
endif

CFLAGS=-O1 -g3 -std=gnu99 -pedantic $(WARNS) -I../libs/libucw/run/include/ -I../libs/lz4/lib/ -rdynamic -pthread # ADD -flto ?
#CFLAGS+= -I../libs/knot/src/

# Using local static libucw and liblz4
LDLIBS=../libs/libucw/run/lib/libucw-6.5.a ../libs/lz4/lib/liblz4.a -ltrace -lpthread -lknot
#LDLIBS+= ../libs/knot/src/libknot.la
LDFLAGS=

PROG=dnscol
DEPS=$(wildcard *.h)
#SRCS=collector.c common.c config.c dns.c main.c output.c output_compression.c packet.c stats.c timeframe.c output_csv.c input.c
SRCS=common.c input.c frame_queue.c packet_frame.c packet_frame_logger.c main.c packet.o
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

