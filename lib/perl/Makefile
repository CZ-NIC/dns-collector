# Perl modules

DIRS+=lib/perl
PROGS+=$(addprefix $(o)/lib/perl/,Config.pm)

ifdef CONFIG_UCW_PERL_ULIMIT
include $(s)/lib/perl/Ulimit/Makefile
endif
