# UCW Library configuration system: documentation requirements
# (c) 2008 Michal Vaner <vorner@ucw.cz>

package UCW::Configure::Doc;
use UCW::Configure;

use strict;
use warnings;

Test("ASCII_DOC", "Checking for AsciiDoc", sub {
	my $version = `asciidoc --version`;
	return "none" if !defined $version || $version eq "";
	my( $vnum ) = $version =~ / (\d+)\.\S*$/;
	return "old" if $vnum < 7;
	return "yes";
});

if(Get("ASCII_DOC") eq "yes") {
	Set("CONFIG_DOC");
} else {
	Warn("Need asciidoc >= 7 to build documentation");
	UnSet("CONFIG_DOC");
}

# We succeeded
1;
