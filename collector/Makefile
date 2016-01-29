.PHONY: all clean

CC=clang
CFLAGS=-g3 -std=gnu99 -Wall -pedantic -Wcast-align -Wunused-parameter -Wno-variadic-macros 
LDFLAGS=-lpcap `pkg-config --libs 'libprotobuf-c >= 1.0.0'` -lucw-6.5
DEPS=$(wildcard *.h)
OBJS=main.o collector.o timeframe.o $(PROTO).pb-c.o stats.o packet.o dns.o writeproto.o
PROG=main
PROTO=dnsquery

all: $(PROG) protodump.py

clean:
	rm -f $(OBJS) $(PROG) $(PROTO)_py2.py $(PROTO).pb-c.c $(PROTO).pb-c.h

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(PROTO).pb-c.c $(PROTO).pb-c.h: $(PROTO).proto
	protoc-c --c_out=. $<

$(PROTO)_py2.py: $(PROTO).proto
	protoc --python_out=. $<

protodump.py: $(PROTO)_py2.py
