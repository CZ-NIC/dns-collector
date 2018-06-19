# Makefile for the UCW utilities (c) 2008 Michal Vaner <vorner@ucw.cz>

DIRS+=ucw/utils

UCW_SBIN_UTILS=$(addprefix $(o)/ucw/utils/ucw-,daemon-control rotate-log)
UCW_BIN_UTILS=$(addprefix $(o)/ucw/utils/ucw-,basecode urltool)

ifdef CONFIG_UCW_OBSOLETE_DAEMON_HELPER
UCW_SBIN_UTILS+=$(o)/ucw/utils/ucw-daemon-helper
endif

PROGS+=$(UCW_BIN_UTILS) $(UCW_SBIN_UTILS)

$(o)/ucw/utils/ucw-basecode: $(o)/ucw/utils/ucw-basecode.o $(LIBUCW)
$(o)/ucw/utils/ucw-daemon-helper: $(o)/ucw/utils/ucw-daemon-helper.o $(LIBUCW)
$(o)/ucw/utils/ucw-daemon-control: $(o)/ucw/utils/ucw-daemon-control.o $(LIBUCW)
$(o)/ucw/utils/ucw-urltool: $(o)/ucw/utils/ucw-urltool.o $(LIBUCW)

TESTS+=$(o)/ucw/utils/basecode.test
$(o)/ucw/utils/basecode.test: $(o)/ucw/utils/ucw-basecode

INSTALL_TARGETS+=install-ucw-utils
install-ucw-utils:
	install -d -m 755 $(addprefix $(DESTDIR),$(INSTALL_BIN_DIR) $(INSTALL_SBIN_DIR))
	install -m 755 $(UCW_SBIN_UTILS) $(DESTDIR)$(INSTALL_SBIN_DIR)
	install -m 755 $(UCW_BIN_UTILS) $(DESTDIR)$(INSTALL_BIN_DIR)
.PHONY: install-ucw-utils
