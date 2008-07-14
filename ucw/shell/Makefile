# Support routines for shell scripts

DIRS+=ucw/shell
PROGS+=$(o)/ucw/shell/config $(o)/ucw/shell/logger
DATAFILES+=$(o)/ucw/shell/libucw.sh

$(o)/ucw/shell/config: $(o)/ucw/shell/config.o $(LIBUCW)
$(o)/ucw/shell/logger: $(o)/ucw/shell/logger.o $(LIBUCW)

TESTS+=$(addprefix $(o)/ucw/shell/,config.test)

$(o)/ucw/shell/config.test: $(o)/ucw/shell/config
