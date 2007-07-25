# Makefile for the Filelock Perl module  (c) 2007 Pavel Chrvat <pchar@ucw.cz>

DIRS+=lib/perl/Filelock/arch/auto/UCW/Filelock
FILELOCK_DIR=lib/perl/Filelock

PROGS+=$(o)/lib/perl/Filelock/Filelock.pm

extras:: $(o)/lib/perl/Filelock/arch/auto/UCW/Filelock/Filelock.$(SOEXT)

$(o)/lib/perl/Filelock/arch/auto/UCW/Filelock/Filelock.$(SOEXT): $(o)/$(FILELOCK_DIR)/Filelock.xs $(o)/$(FILELOCK_DIR)/Filelock.pm $(o)/$(FILELOCK_DIR)/Makefile
	$(M)MAKE $@
	$(Q)cd $(o)/$(FILELOCK_DIR) && $(MAKE) -f Makefile $(MAKESILENT)
	$(Q)touch $@
	$(Q)cp $@ run/$(DATADIR)/

$(o)/$(FILELOCK_DIR)/Makefile: $(o)/$(FILELOCK_DIR)/Makefile.PL
	$(M)PREPARE $@
	$(Q)cd $(o)/$(FILELOCK_DIR) && perl Makefile.PL

$(o)/$(FILELOCK_DIR)/Filelock.xs: $(s)/$(FILELOCK_DIR)/Filelock.xs
	$(Q)cp $^ $@

$(o)/$(FILELOCK_DIR)/Makefile.PL: $(s)/$(FILELOCK_DIR)/Makefile.PL
	$(Q)cp $^ $@
