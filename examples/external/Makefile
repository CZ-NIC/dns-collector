# Example Makefile for a stand-alone program using libucw

CFLAGS:=$(shell pkg-config --cflags libucw)
LDLIBS:=$(shell pkg-config --libs libucw)

all: test

test: test.c
