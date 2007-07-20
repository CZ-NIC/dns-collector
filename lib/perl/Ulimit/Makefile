# Makefile for the Ulimit Perl module  (c) 2003 Tomas Valla <tom@ucw.cz>

DIRS+=lib/perl/Ulimit/arch/auto/UCW/Ulimit
ULIMIT_DIR=lib/perl/Ulimit

PROGS+=$(o)/lib/perl/Ulimit/Ulimit.pm

extras:: $(o)/lib/perl/Ulimit/arch/auto/UCW/Ulimit/Ulimit.$(SOEXT)

$(o)/lib/perl/Ulimit/arch/auto/UCW/Ulimit/Ulimit.$(SOEXT): $(o)/$(ULIMIT_DIR)/Ulimit.xs $(o)/$(ULIMIT_DIR)/Ulimit.pm $(o)/$(ULIMIT_DIR)/Makefile
	$(M)MAKE $@
	$(Q)cd $(o)/$(ULIMIT_DIR) && $(MAKE) -f Makefile $(MAKESILENT)
	$(Q)cp $@ run/$(DATADIR)/

$(o)/$(ULIMIT_DIR)/Makefile: $(o)/$(ULIMIT_DIR)/Makefile.PL
	$(M)PREPARE $@
	$(Q)cd $(o)/$(ULIMIT_DIR) && perl Makefile.PL

$(o)/$(ULIMIT_DIR)/Ulimit.xs: $(s)/$(ULIMIT_DIR)/Ulimit.xs
	$(Q)cp $^ $@

$(o)/$(ULIMIT_DIR)/Makefile.PL: $(s)/$(ULIMIT_DIR)/Makefile.PL
	$(Q)cp $^ $@
