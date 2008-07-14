# Perl modules

DIRS+=ucw/perl
EXTRA_RUNDIRS+=lib/perl5/UCW
PROGS+=$(addprefix $(o)/ucw/perl/,Config.pm Log.pm CGI.pm)

ifdef CONFIG_UCW_PERL_MODULES
include $(s)/ucw/perl/Ulimit/Makefile
include $(s)/ucw/perl/Filelock/Makefile
endif
