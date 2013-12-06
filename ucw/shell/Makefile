# Support routines for shell scripts

DIRS+=ucw/shell
UCW_SHELL_PROGS=$(addprefix $(o)/ucw/shell/ucw-,config logger logoutput)
PROGS+=$(UCW_SHELL_PROGS)
DATAFILES+=$(o)/ucw/shell/libucw.sh

$(o)/ucw/shell/ucw-config: $(o)/ucw/shell/ucw-config.o $(LIBUCW)
$(o)/ucw/shell/ucw-logger: $(o)/ucw/shell/ucw-logger.o $(LIBUCW)
$(o)/ucw/shell/ucw-logoutput: $(o)/ucw/shell/ucw-logoutput.o $(LIBUCW)

TESTS+=$(addprefix $(o)/ucw/shell/,config.test)

$(o)/ucw/shell/config.test: $(o)/ucw/shell/ucw-config

INSTALL_TARGETS+=install-ucw-shell
install-ucw-shell:
	install -d -m 755 $(DESTDIR)$(INSTALL_BIN_DIR)
	install -m 755 $(UCW_SHELL_PROGS) $(DESTDIR)$(INSTALL_BIN_DIR)

.PHONY: install-ucw-shell
