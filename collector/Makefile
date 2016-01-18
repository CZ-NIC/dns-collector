.PHONY: all clean

CC=clang
CFLAGS=-g3 -std=gnu99 -Wall -pedantic
LDFLAGS=-lpcap
DEPS=$(wildcard *.h)
OBJS=main.o collector.o timeframe.o
PROG=main

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

