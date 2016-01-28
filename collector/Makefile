.PHONY: all clean

CC=clang
CFLAGS=-g3 -std=gnu99 -Wall -pedantic -Wcast-align -Wunused-parameter -Wno-variadic-macros -I ../libucw/run/include
LDFLAGS=-lpcap -lprotobuf `pkg-config --libs 'libprotobuf-c >= 1.0.0'` -L ../libucw/run/lib -lucw-6.5
DEPS=$(wildcard *.h)
OBJS=main.o collector.o timeframe.o exchange.pb-c.o stats.o packet.o dns.o
PROG=main

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG) exchange.pb-c.h exchange.pb-c.c

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

exchange.pb-c.c exchange.pb-c.h: exchange.proto
	protoc-c --c_out=. $<


