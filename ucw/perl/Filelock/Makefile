# Makefile for the Filelock Perl module  (c) 2007 Pavel Chrvat <pchar@ucw.cz>

DIRS+=ucw/perl/Filelock/arch/auto/UCW/Filelock
FILELOCK_DIR=ucw/perl/Filelock

PROGS+=$(o)/ucw/perl/Filelock/Filelock.pm

extras:: $(o)/ucw/perl/Filelock/arch/auto/UCW/Filelock/Filelock.$(SOEXT)

$(o)/ucw/perl/Filelock/arch/auto/UCW/Filelock/Filelock.$(SOEXT): $(o)/$(FILELOCK_DIR)/Filelock.xs $(o)/$(FILELOCK_DIR)/Filelock.pm $(o)/$(FILELOCK_DIR)/Makefile
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
