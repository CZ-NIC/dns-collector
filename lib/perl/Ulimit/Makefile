# Makefile for the Ulimit Perl module  (c) 2003 Tomas Valla <tom@ucw.cz>

DIRS+=lib/perl/Ulimit/arch/auto/Sherlock/Ulimit
ULIMIT_DIR=lib/perl/Ulimit

PROGS+=$(o)/lib/perl/Ulimit/Ulimit.pm

extras:: $(o)/lib/perl/Ulimit/arch/auto/Sherlock/Ulimit/Ulimit.$(SOEXT)

$(o)/lib/perl/Ulimit/arch/auto/Sherlock/Ulimit/Ulimit.$(SOEXT): $(o)/$(ULIMIT_DIR)/Ulimit.xs $(o)/$(ULIMIT_DIR)/Ulimit.pm $(o)/$(ULIMIT_DIR)/Makefile
	cd $(o)/$(ULIMIT_DIR) && $(MAKE) -f Makefile
	cp $@ run/$(DATADIR)/

$(o)/$(ULIMIT_DIR)/Makefile: $(o)/$(ULIMIT_DIR)/Makefile.PL
	cd $(o)/$(ULIMIT_DIR) && perl Makefile.PL

$(o)/$(ULIMIT_DIR)/Ulimit.xs: $(s)/$(ULIMIT_DIR)/Ulimit.xs
	cp $^ $@

$(o)/$(ULIMIT_DIR)/Makefile.PL: $(s)/$(ULIMIT_DIR)/Makefile.PL
	cp $^ $@
