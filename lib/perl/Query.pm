#	Perl module for sending queries to Sherlock search servers and parsing answers
#
#	(c) 2002 Martin Mares <mj@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

package Sherlock::Query;

use strict;
use warnings;
use IO::Socket::INET;

sub parse_tree($$);
sub do_parse_tree($$$$);
sub format_tree($$$);

sub new($$) {
	my $class = shift @_;
	my $server = shift @_;
	my $self = {
		SERVER	=> $server
	};
	bless $self;
	return $self;
}

sub command($$) {
	my ($q,$string) = @_;

	$q->{RAW} = [];

	my $sock = IO::Socket::INET->new(PeerAddr => $q->{SERVER}, Proto => 'tcp')
		or return "-900 Cannot connect to search server: $!";
	print $sock $string, "\n";

	# Status line
	my $stat = <$sock>;
	chomp $stat;
	$stat =~ /^[+-]/ or return "-901 Reply parse error";

	# Blocks of output
	my $block = undef;
	for(;;) {
		my $res = <$sock>;
		last if $sock->eof;
		chomp $res;
		if ($res eq "") {
			$block = undef;
		} else {
			if (!defined $block) {
				$block = [];
				push @{$q->{RAW}}, $block;
			}
			push @$block, $res;
		}
	}

	return $stat;
}

our $hdr_syntax = {
	'D' => {
		'D' => "",
		'W' => [],
		'P' => [],
		'n' => [],
		'T' => "",
		'-' => "",
		'.' => [],
	},
	'.' => [],
	'' => ""
};

our $card_syntax = {
	'U' => {
		'U' => "",
		'D' => "",
		'E' => "",
		'L' => "",
		'T' => "",
		'c' => "",
		's' => "",
		'V' => [],
		'b' => "",
		'i' => "",
		'y' => [],
		'z' => "",
	},
	'M' => [],
	'X' => [],
	'' => ""
};

our $footer_syntax = {
	'' => ""
};

sub query($$) {
	my ($q,$string) = @_;

	# Send the query and gather results
	my $stat = $q->command($string);
	my @raw = @{$q->{RAW}};

	# Split results to header, cards and footer
	$q->{HEADER} = { RAW => [] };
	if (@raw) { $q->{HEADER}{RAW} = shift @raw; }
	elsif (!$stat) { return "-902 Incomplete reply"; }
	$q->{FOOTER} = { RAW => [] };
	if (@raw && $raw[@raw-1]->[0] =~ /^\+/) {
		$q->{FOOTER}{RAW} = pop @raw;
	}
	$q->{CARDS} = [];
	while (@raw) {
		push @{$q->{CARDS}}, { RAW => pop @raw };
	}

	# Parse everything
	parse_tree($q->{HEADER}, $hdr_syntax);
	foreach my $c (@{$q->{CARDS}}) {
		parse_tree($c, $card_syntax);
	}
	parse_tree($q->{FOOTER}, $footer_syntax);

	return $stat;
}

sub parse_tree($$) {
	my $tree = shift @_;
	my $syntax = shift @_;
	do_parse_tree($tree->{RAW}, 0, $tree, $syntax);
}

sub do_parse_tree($$$$) {
	my $raw = shift @_;
	my $i = shift @_;
	my $cooked = shift @_;
	my $syntax = shift @_;

	while ($i < @$raw) {
		$raw->[$i] =~ /^(.)(.*)/;
		if (!defined($syntax->{$1}) && !defined($syntax->{''})) { return $i; }
		if (ref $syntax->{$1} eq "ARRAY") {
			push @{$cooked->{$1}}, $2;
			$i++;
		} elsif (ref $syntax->{$1} eq "HASH") {
			my $block = {};
			push @{$cooked->{$1}}, $block;
			$i = do_parse_tree($raw, $i, $block, $syntax->{$1});
		} else {
			$cooked->{$1} = $2 if !defined($cooked->{$1});
			$i++;
		}
	}
}

sub format_tree($$$) {
	my ($func, $a, $indent) = @_;
	if (ref $a eq "ARRAY") {
		if (@{$a} == 0) { &$func("[]\n"); }
		else {
			&$func("[\n");
			foreach my $k (@{$a}) {
				&$func("$indent\t");
				format_tree($func, $k, "$indent\t");
			}
			&$func($indent . "]\n");
		}
	} elsif (ref $a) {
		&$func("{\n");
		foreach my $k (sort keys %{$a}) {
			&$func("$indent\t$k => ");
			format_tree($func, $a->{$k}, "$indent\t");
		}
		&$func($indent . "}\n");
	} elsif (defined $a) {
		&$func("$a\n");
	} else {
		&$func("UNDEF\n");
	}
}

sub format($&$) {
	my ($q, $func, $what) = @_;
	format_tree($func, $what, "");
}

sub print($) {
	my $q = shift @_;
	format_tree(sub { print $_[0]; }, $q, "");
}

1;  # OK
