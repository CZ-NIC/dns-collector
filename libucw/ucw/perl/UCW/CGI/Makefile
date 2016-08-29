# More CGI support

DIRS+=ucw/perl/UCW/CGI
EXTRA_RUNDIRS+=lib/perl5/UCW/CGI
UCW_CGI_PERL_MODULES+=ErrorHandler.pm
CGI_MODULES=$(addprefix $(o)/ucw/perl/UCW/CGI/,$(UCW_CGI_PERL_MODULES))
PROGS+=$(CGI_MODULES)

$(CGI_MODULES) : PERL_MODULE_DIR=UCW/CGI

INSTALL_TARGETS+=install-perl-ucw-cgi
install-perl-ucw-cgi:
	install -d -m 755 $(DESTDIR)$(INSTALL_PERL_DIR)/UCW/CGI
	install -m 644 $(addprefix $(s)/ucw/perl/UCW/CGI/,$(UCW_CGI_PERL_MODULES)) $(DESTDIR)$(INSTALL_PERL_DIR)/UCW/CGI
.PHONY: install-perl-ucw-cgi
