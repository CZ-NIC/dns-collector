.PHONY: all clean

CC=clang
CFLAGS=-O2 -g3 -std=gnu99 -Wall -pedantic -Wcast-align -Wunused-parameter -Wno-variadic-macros 
LDFLAGS=-lpcap `pkg-config --libs 'libprotobuf-c >= 1.0.0'` -lucw-6.5
DEPS=$(wildcard *.h) dnsquery.pb-c.h
OBJS=main.o collector.o timeframe.o $(PROTO).pb-c.o stats.o packet.o dns.o writeproto.o
PROG=main
PROTO=dnsquery

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
