#	Perl module for sending queries to Sherlock search servers and parsing answers
#
#	(c) 2002--2003 Martin Mares <mj@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

=head1 NAME

Sherlock::Query -- Communication with Sherlock Search Servers

=head1 DESCRIPTION

This perl library offers a simple interface for connecting to Sherlock
search servers, sending queries or control commands and parsing the
results.

First of all, you have to use

	my $conn = new Sherlock::Query('server:port');

to create a new connection object (unconnected yet). Then you can call

	my $res = $conn->command('command');

to establish the connection, send a given command to the search server
and gather the results (see below) or, if you want to send a normal query,

	my $res = $conn->query('"simple" OR "query"');

which does the same as C<< $conn->command(...) >>, but it also parses the
results to a representation convenient for handling in Perl programs
(again, see below).

Currently, you can use a single connection to send only a single command or query.

=head1 RESULTS

The I<raw answer> of the search server (i.e., the lines it has returned) is always
available as C<< $conn->{RAW} >> as a list of strings, each representing a single
line.

Parsed results of queries are stored in a more complicated way, but before
explaining it, let's mention a couple of axioms: Any search server I<object>
(header, footer, a single document of answer) is always stored as a hash keyed
by attribute names. Ordinary single-valued attributes are stored as strings,
multi-valued attributes as (references to) arrays of strings. When an object
contains sub-objects, they are stored as references to other hashes, possibly
encapsulated within a list if there can be multiple such objects. Most objects
have an extra attribute C<RAW> containing the original description of the
object, a sub-list of C<< $conn->{RAW} >>.

The parsed answer consists of three parts (please follow F<doc/search> to
get a better picture of what does the server answer): header C<< $conn->{HEADER} >>
(an object, as described above), footer C<< $conn->{FOOTER} >> (object) and document
cards C<< $conn->{CARDS} >> (a list of objects).

The I<header> contains all the standard header attributes and also C<< $hdr->{D} >>
which is a list of sub-objects, each corresponding to a single database and
containing per-database attributes like C<W> (word list).

The I<footer> is pretty straightforward and it just contains what you'd
expect it to.

Each I<card> contains the usual document attributes (see F<doc/objects> for
a list) plus C<< $card->{U} >> which is a list of sub-objects corresponding
to URL's of the document and containing per-URL attributes like C<U> (URL),
C<s> (original size) and C<T> (content type).

When in doubt, call the C<print> method which will print the whole contents
of the connection object. It's actually a much more general (but pretty
simple due to Perl being able to be a very introspective language) routine
usable for dumping any acyclic Perl data structure composed of strings,
hashes, arrays and references to them. You can access this general routine
by calling C<format({ print; }, $what)> which dumps C<$what> and for
each line of output it calls the given subroutine.

=head1 SEE ALSO

A good example of use of this module is the C<query> utility and
of course the example front-end (F<front-end/query.cgi>).

=head1 AUTHOR

Martin Mares <mj@ucw.cz>

=cut

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
	$stat = "-903 Incomplete reply" if !defined $stat;
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
	'(D' => {
		'W' => [],
		'P' => [],
		'n' => [],
		'.' => [],
	},
	'.' => [],
};

our $card_syntax = {
	'(U' => {
		'V' => [],
		'y' => [],
		'E' => [],
	},
	'M' => [],
	'X' => [],
};

our $footer_syntax = {
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
		push @{$q->{CARDS}}, { RAW => shift @raw };
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
		$raw->[$i] =~ /^([^(]|\(.)(.*)/;
		if ($1 eq ")") {
			return $i;
		} elsif (!defined($syntax->{$1})) {
			$cooked->{$1} = $2 if !defined($cooked->{$1});
			$i++;
		} elsif (ref $syntax->{$1} eq "ARRAY") {
			push @{$cooked->{$1}}, $2;
			$i++;
		} elsif (ref $syntax->{$1} eq "HASH") {
			my $block = {};
			push @{$cooked->{$1}}, $block;
			$i = do_parse_tree($raw, $i+1, $block, $syntax->{$1});
		}
	}
	return $i;
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
