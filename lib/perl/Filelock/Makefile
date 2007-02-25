# Makefile for the Filelock Perl module  (c) 2007 Pavel Chrvat <pchar@ucw.cz>

DIRS+=lib/perl/Filelock/arch/auto/Sherlock/Filelock
FILELOCK_DIR=lib/perl/Filelock

PROGS+=$(o)/lib/perl/Filelock/Filelock.pm
ifdef CONFIG_DARWIN
SOEXT=bundle
else
SOEXT=so
endif

extras:: $(o)/lib/perl/Filelock/arch/auto/Sherlock/Filelock/Filelock.$(SOEXT)

$(o)/lib/perl/Filelock/arch/auto/Sherlock/Filelock/Filelock.$(SOEXT): $(o)/$(FILELOCK_DIR)/Filelock.xs $(o)/$(FILELOCK_DIR)/Filelock.pm $(o)/$(FILELOCK_DIR)/Makefile
	cd $(o)/$(FILELOCK_DIR) && $(MAKE) -f Makefile
	cp $@ run/$(DATADIR)/

$(o)/$(FILELOCK_DIR)/Makefile: $(o)/$(FILELOCK_DIR)/Makefile.PL
	cd $(o)/$(FILELOCK_DIR) && perl Makefile.PL

$(o)/$(FILELOCK_DIR)/Filelock.xs: $(s)/$(FILELOCK_DIR)/Filelock.xs
	cp $^ $@

$(o)/$(FILELOCK_DIR)/Makefile.PL: $(s)/$(FILELOCK_DIR)/Makefile.PL
	cp $^ $@
