# Makefile for the Ulimit Perl module  (c) 2003 Tomas Valla <tom@ucw.cz>

DIRS+=lib/perl/Ulimit/arch/auto/Sherlock/Ulimit
ULIMIT_DIR=lib/perl/Ulimit

PROGS+=$(o)/lib/perl/Ulimit/Ulimit.pm

extras:: $(o)/lib/perl/Ulimit/arch/auto/Sherlock/Ulimit/Ulimit.so

$(o)/lib/perl/Ulimit/arch/auto/Sherlock/Ulimit/Ulimit.so: $(o)/$(ULIMIT_DIR)/Ulimit.xs $(o)/$(ULIMIT_DIR)/Ulimit.pm $(o)/$(ULIMIT_DIR)/Makefile
	cd $(o)/$(ULIMIT_DIR) && $(MAKE) -f Makefile
	cp $@ run/$(DATADIR)/

$(o)/$(ULIMIT_DIR)/Makefile: $(o)/$(ULIMIT_DIR)/Makefile.PL
	cd $(o)/$(ULIMIT_DIR) && perl Makefile.PL

$(o)/$(ULIMIT_DIR)/%: $(s)/$(ULIMIT_DIR)/%
	cp $^ $@
