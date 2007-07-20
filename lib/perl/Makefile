# Perl modules

DIRS+=lib/perl
EXTRA_RUNDIRS+=lib/perl5/UCW
PROGS+=$(addprefix $(o)/lib/perl/,Config.pm Log.pm)

ifdef CONFIG_UCW_PERL_MODULES
include $(s)/lib/perl/Ulimit/Makefile
include $(s)/lib/perl/Filelock/Makefile
endif
