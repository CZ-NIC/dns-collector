# Perl module for parsing Sherlock configuration files (using the config utility)
# (c) 2002 Martin Mares <mj@ucw.cz>

package Sherlock::Config;

use strict;
use warnings;
use Getopt::Long;

our %Sections = ();

our $DefaultConfigFile = "";

sub Parse(@) {
	my @options = @_;
	my $defargs = "";
	my $override_config = 0;
	push @options, "config|C=s" => sub { my ($o,$a)=@_; $defargs .= " -C'$a'"; $override_config=1; };
	push @options, "set|S=s" => sub { my ($o,$a)=@_; $defargs .= " -S'$a'"; };
	Getopt::Long::GetOptions(@options) or return 0;
	if (!$override_config && $DefaultConfigFile) {
		$defargs = "-C'$DefaultConfigFile' $defargs";
	}
	foreach my $section (keys %Sections) {
		my $opts = $Sections{$section};
		my $optlist = join(" ", keys %$opts);
		my @l = `bin/config $defargs $section $optlist`;
		$? && exit 1;
		foreach my $o (@l) {
			$o =~ /^CF_([^=]+)="(.*)"\n$/ or die "Cannot parse bin/config output: $_";
			my $var = $$opts{$1};
			my $val = $2;
			if (ref $var eq "SCALAR") {
				$$var = $val;
			} elsif (ref $var eq "ARRAY") {
				push @$var, $val;
			} elsif (ref $var) {
				die ("Sherlock::Config::Parse: don't know how to set $o");
			}
		}
	}
	1;
}

1;  # OK
