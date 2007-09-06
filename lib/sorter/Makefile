# Makefile for the UCW Sorter (c) 2007 Martin Mares <mj@ucw.cz>

DIRS+=lib/sorter

LIBUCW_MODS+=$(addprefix sorter/, config govern sbuck array)
PROGS+=$(o)/lib/sorter/sort-test $(o)/lib/sorter/old-test

$(o)/lib/sorter/sort-test: $(o)/lib/sorter/sort-test.o $(LIBUCW)
$(o)/lib/sorter/old-test: $(o)/lib/sorter/old-test.o $(LIBUCW)
