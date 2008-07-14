# Makefile for the UCW Sorter (c) 2007 Martin Mares <mj@ucw.cz>

DIRS+=ucw/sorter

LIBUCW_MODS+=$(addprefix sorter/, config govern sbuck array)
LIBUCW_INCLUDES+=$(addprefix sorter/, array.h common.h s-fixint.h \
	s-internal.h s-multiway.h s-radix.h s-twoway.h sorter.h)

ifdef CONFIG_DEBUG_TOOLS
PROGS+=$(o)/ucw/sorter/sort-test
endif

$(o)/ucw/sorter/sort-test: $(o)/ucw/sorter/sort-test.o $(LIBUCW)
