# Makefile for the Ulimit Perl module  (c) 2003 Tomas Valla <tom@ucw.cz>

DIRS+=lib/perl/Ulimit/arch/auto/Sherlock/Ulimit


ULIMIT_DIR=lib/perl/Ulimit

PROGS+=obj/lib/perl/Ulimit/Ulimit.pm
DATAFILES+=obj/lib/perl/Ulimit/arch/auto/Sherlock/Ulimit/Ulimit.so


lib/perl/Ulimit/arch/auto/Sherlock/Ulimit/Ulimit.so: $(ULIMIT_DIR)/Ulimit.xs $(ULIMIT_DIR)/Ulimit.pm $(ULIMIT_DIR)/Makefile.tmp
	cd $(ULIMIT_DIR) && $(MAKE) -f Makefile.tmp

$(ULIMIT_DIR)/Makefile.tmp: $(ULIMIT_DIR)/Makefile.PL
	cd $(ULIMIT_DIR) && perl Makefile.PL


clean::
	cd $(ULIMIT_DIR) && if [ -f Makefile.tmp ]; then $(MAKE) -f Makefile.tmp distclean; fi && rm -rf lib arch


