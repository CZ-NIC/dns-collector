# Support routines for shell scripts

DIRS+=lib/shell
PROGS+=$(o)/lib/shell/config $(o)/lib/shell/logger
DATAFILES+=$(o)/lib/shell/libucw.sh

$(o)/lib/shell/config: $(o)/lib/shell/config.o $(LIBUCW)
$(o)/lib/shell/logger: $(o)/lib/shell/logger.o $(LIBUCW)

TESTS+=$(addprefix $(o)/lib/shell/,config.test)

$(o)/lib/shell/config.test: $(o)/lib/shell/config
