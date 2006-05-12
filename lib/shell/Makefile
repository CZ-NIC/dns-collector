# Support routines for shell scripts

DIRS+=lib/shell
PROGS+=$(o)/lib/shell/config $(o)/lib/shell/logger
DATAFILES+=$(o)/lib/shell/libucw.sh

$(o)/lib/shell/config: $(o)/lib/shell/config.o $(LIBSH)
$(o)/lib/shell/logger: $(o)/lib/shell/logger.o $(LIBSH)

TESTS+=$(addprefix $(o)/lib/shell/,config.test)
