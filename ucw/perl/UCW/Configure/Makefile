# Perl modules for Configure

DIRS+=ucw/perl/UCW/Configure
EXTRA_RUNDIRS+=lib/perl5/UCW/Configure
CONFIGURE_MODULES=$(addprefix $(o)/ucw/perl/UCW/Configure/,C.pm Doc.pm Paths.pm Pkg.pm)
PROGS+=$(CONFIGURE_MODULES)

$(CONFIGURE_MODULES) : PERL_MODULE_DIR=UCW/Configure
