#	Poor Man's CGI Module for Perl
#
#	(c) 2002--2007 Martin Mares <mj@ucw.cz>
#	Slightly modified by Tomas Valla <tom@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

package UCW::CGI;

use strict;
use warnings;

BEGIN {
	# The somewhat hairy Perl export mechanism
	use Exporter();
	our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
	$VERSION = 1.0;
	@ISA = qw(Exporter);
	@EXPORT = qw(&html_escape &url_escape &self_ref &self_form);
	@EXPORT_OK = qw();
	%EXPORT_TAGS = ();
}

sub url_escape($) {
	my $x = shift @_;
	$x =~ s/([^-\$_.!*'(),0-9A-Za-z\x80-\xff])/"%".unpack('H2',$1)/ge;
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

### Analysing RFC 822 Style Headers ###

sub rfc822_prepare($) {
	my $x = shift @_;
	# Convert all %'s and backslash escapes to %xx escapes
	$x =~ s/%/%25/g;
	$x =~ s/\\(.)/"%".unpack("H2",$1)/ge;
	# Remove all comments, beware, they can be nested (unterminated comments are closed at EOL automatically)
	while ($x =~ s/^(("[^"]*"|[^"(])*(\([^)]*)*)(\([^()]*(\)|$))/$1 /) { }
	# Remove quotes and escape dangerous characters inside (again closing at the end automatically)
	$x =~ s{"([^"]*)("|$)}{my $z=$1; $z =~ s/([^0-9a-zA-Z%_-])/"%".unpack("H2",$1)/ge; $z;}ge;
	# All control characters are properly escaped, tokens are clearly visible.
	# Finally remove all unnecessary spaces.
	$x =~ s/\s+/ /g;
	$x =~ s/(^ | $)//g;
	$x =~ s{\s*([()<>@,;:\\"/\[\]?=])\s*}{$1}g;
	return $x;
}

sub rfc822_deescape($) {
	my $x = shift @_;
	$x =~ s/%(..)/pack("H2",$1)/ge;
	return $x;
}

### Reading of HTTP headers ###

sub http_get($) {
	my $h = shift @_;
	$h =~ tr/a-z-/A-Z_/;
	return $ENV{"HTTP_$h"} || $ENV{"$h"};
}

### Parsing of Arguments ###

our $arg_table;

sub parse_arg_string($) {
	my ($s) = @_;
	$s =~ s/\s+//;
	foreach $_ (split /[&:]/,$s) {
		(/^([^=]+)=(.*)$/) or next;
		my $arg = $arg_table->{$1} or next;
		$_ = $2;
		s/\+/ /g;
		s/%(..)/pack("c",hex $1)/eg;
		s/(\r|\n|\t)/ /g;
		s/^\s+//;
		s/\s+$//;
		if (my $rx = $arg->{'check'}) {
			if (!/^$rx$/) { $_ = $arg->{'default'}; }
		}

		my $r = ref($arg->{'var'});
		if ($r eq 'SCALAR') {
			${$arg->{'var'}} = $_;
		} elsif ($r eq 'ARRAY') {
			push @{$arg->{'var'}}, $_;
		}
	}
}

sub parse_args($) {
	$arg_table = shift @_;
	foreach my $a (values %$arg_table) {
		my $r = ref($a->{'var'});
		defined($a->{'default'}) or $a->{'default'}="";
		if ($r eq 'SCALAR') {
			${$a->{'var'}} = $a->{'default'};
		} elsif ($r eq 'ARRAY') {
			@{$a->{'var'}} = ();
		}
	}
	defined $ENV{"GATEWAY_INTERFACE"} or die "Not called as a CGI script";
	my $method = $ENV{"REQUEST_METHOD"};
	if ($method eq "GET") {
		parse_arg_string($ENV{"QUERY_STRING"});
	} elsif ($method eq "POST") {
		if ($ENV{"CONTENT_TYPE"} =~ /^application\/x-www-form-urlencoded\b/i) {
			while (<STDIN>) {
				chomp;
				parse_arg_string($_);
			}
		} else {
			return "Unknown content type for POST data";
		}
	} else {
		return "Unknown request method";
	}
}

### Generating Self-ref URL's ###

sub make_out_args($) {
	my ($overrides) = @_;
	my $out = {};
	foreach my $name (keys %$arg_table) {
		my $arg = $arg_table->{$name};
		defined($arg->{'var'}) || next;
		defined($arg->{'pass'}) && !$arg->{'pass'} && !exists $overrides->{$name} && next;
		my $value;
		if (!defined($value = $overrides->{$name})) {
			if (exists $overrides->{$name}) {
				$value = $arg->{'default'};
			} else {
				$value = ${$arg->{'var'}};
			}
		}
		if ($value ne $arg->{'default'}) {
			$out->{$name} = $value;
		}
	}
	return $out;
}

sub self_ref(@) {
	my %h = @_;
	my $out = make_out_args(\%h);
	return "?" . join(':', map { "$_=" . url_escape($out->{$_}) } sort keys %$out);
}

sub self_form(@) {
	my %h = @_;
	my $out = make_out_args(\%h);
	return join('', map { "<input type=hidden name=$_ value='" . html_escape($out->{$_}) . "'>\n" } sort keys %$out);
}

### Cookies

sub cookie_esc($) {
	my $x = shift @_;
	if ($x !~ /^[a-zA-Z0-9%]+$/) {
		$x =~ s/([\\\"])/\\$1/g;
		$x = "\"$x\"";
	}
	return $x;
}

sub set_cookie($$@) {
	my $key = shift @_;
	my $value = shift @_;
	my %other = @_;
	$other{'version'} = 1 unless defined $other{'version'};
	print "Set-Cookie: $key=", cookie_esc($value);
	foreach my $k (keys %other) {
		print ";$k=", cookie_esc($other{$k});
	}
	print "\n";
}

sub parse_cookies() {
	my $h = http_get("Cookie") or return ();
	my @cook = ();
	while (my ($padding,$name,$val,$xx,$rest) = ($h =~ /\s*([,;]\s*)*([^ =]+)=([^ =,;\"]*|\"([^\"\\]|\\.)*\")(\s.*|;.*|$)/)) {
		if ($val =~ /^\"/) {
			$val =~ s/^\"//;
			$val =~ s/\"$//;
			$val =~ s/\\(.)/$1/g;
		}
		push @cook, $name, $val;
		$h = $rest;
	}
	return @cook;
}

1;  # OK
