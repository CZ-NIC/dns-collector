# Makefile for the UCW utilities (c) 2008 Michal Vaner <vorner@ucw.cz>

UCW_UTILS=$(addprefix $(o)/ucw/utils/ucw-,basecode daemon-control rotate-log urltool)
PROGS+=$(UCW_UTILS)
DIRS+=ucw/utils

ifdef CONFIG_UCW_OBSOLETE_DAEMON_HELPER
PROGS+=$(o)/ucw/utils/ucw-daemon-helper
endif

$(o)/ucw/utils/ucw-basecode: $(o)/ucw/utils/ucw-basecode.o $(LIBUCW)
$(o)/ucw/utils/ucw-daemon-helper: $(o)/ucw/utils/ucw-daemon-helper.o $(LIBUCW)
$(o)/ucw/utils/ucw-daemon-control: $(o)/ucw/utils/ucw-daemon-control.o $(LIBUCW)
$(o)/ucw/utils/ucw-urltool: $(o)/ucw/utils/ucw-urltool.o $(LIBUCW)

TESTS+=$(o)/ucw/utils/basecode.test
$(o)/ucw/utils/basecode.test: $(o)/ucw/utils/ucw-basecode

INSTALL_UTILS_TARGETS+=install-ucw-utils
install-ucw-utils:
	install -d -m 755 $(DESTDIR)$(INSTALL_BIN_DIR)
	install -m 755 $(UCW_UTILS) $(DESTDIR)$(INSTALL_BIN_DIR)
.PHONY: install-ucw-utils
