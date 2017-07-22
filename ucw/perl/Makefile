# Perl modules

DIRS+=ucw/perl
UCW_PERL_INSTALL=

include $(s)/ucw/perl/UCW/Makefile

ifdef CONFIG_UCW_PERL_MODULES
include $(s)/ucw/perl/Ulimit/Makefile
include $(s)/ucw/perl/Filelock/Makefile
endif

INSTALL_TARGETS+=install-perl-ucw

install-perl-ucw: $(UCW_PERL_INSTALL)
.PHONY: install-perl-ucw
