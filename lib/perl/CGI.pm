#	Poor Man's CGI Module for Perl
#
#	(c) 2002 Martin Mares <mj@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

package Sherlock::CGI;

use strict;
use warnings;

BEGIN {
	# The somewhat hairy Perl export mechanism
	use Exporter();
	our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
	$VERSION = 1.0;
	@ISA = qw(Exporter);
	@EXPORT = qw(&html_escape &url_escape);
	@EXPORT_OK = qw();
	%EXPORT_TAGS = ();
}

sub url_escape($) {
	my $x = shift @_;
	$x =~ s/([^-\$_.+!*'(),0-9A-Za-z\x80-\xff])/"%".unpack('H2',$1)/ge;
	return $x;
}

sub html_escape($) {
	my $x = shift @_;
	$x =~ s/&/&amp;/g;
	$x =~ s/</&lt;/g;
	$x =~ s/>/&gt;/g;
	$x =~ s/"/&quot;/g;
	return $x;
}

1;  # OK
