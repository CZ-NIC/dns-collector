# Makefile for the UCW utilities (c) 2008 Michal Vaner <vorner@ucw.cz>

PROGS+=$(addprefix $(o)/ucw/utils/,b224 basecode daemon-helper rotate-log urltool)
DIRS+=ucw/utils

ifdef CONFIG_DEBUG
PROGS+=$(o)/ucw/utils/hex
endif

$(o)/ucw/utils/b224: $(o)/ucw/utils/b224.o $(LIBUCW)
$(o)/ucw/utils/basecode: $(o)/ucw/utils/basecode.o $(LIBUCW)
$(o)/ucw/utils/daemon-helper: $(o)/ucw/utils/daemon-helper.o $(LIBUCW)
$(o)/ucw/utils/hex: $(o)/ucw/utils/hex.o $(LIBUCW)
$(o)/ucw/utils/urltool: $(o)/ucw/utils/urltool.o $(LIBUCW)

TESTS+=$(o)/ucw/utils/basecode.test
$(o)/ucw/utils/basecode.test: $(o)/ucw/utils/basecode
