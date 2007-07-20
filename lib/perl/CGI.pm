#	Poor Man's CGI Module for Perl
#
#	(c) 2002 Martin Mares <mj@ucw.cz>
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

sub make_out_args($) {
	my ($overrides) = @_;
	my $out = {};
	foreach my $name (keys %$arg_table) {
		my $arg = $arg_table->{$name};
		defined $arg->{'pass'} && !$arg->{'pass'} && !exists $overrides->{$name} && next;
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

1;  # OK
