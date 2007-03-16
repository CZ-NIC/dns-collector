# Perl modules

DIRS+=lib/perl
PROGS+=$(addprefix $(o)/lib/perl/,Config.pm Log.pm)

ifdef CONFIG_UCW_PERL_ULIMIT
include $(s)/lib/perl/Ulimit/Makefile
endif

ifdef CONFIG_UCW_PERL_FILELOCK
include $(s)/lib/perl/Filelock/Makefile
endif
