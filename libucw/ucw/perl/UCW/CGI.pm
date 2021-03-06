#	Poor Man's CGI Module for Perl
#
#	(c) 2002--2017 Martin Mares <mj@ucw.cz>
#	Slightly modified by Tomas Valla <tom@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

package UCW::CGI;

use strict;
use warnings;

require Exporter;
our $VERSION = 1.0;
our @ISA = qw(Exporter);
our @EXPORT = qw(&html_escape &url_escape &url_deescape &url_param_escape &url_param_deescape &self_ref &self_form &http_get);
our @EXPORT_OK = qw();

# Configuration settings
our $debug = 0;
our $utf8_mode = 0;

sub http_error($;@) {
	my $err = shift @_;
	print join("\n", "Status: $err", "Content-Type: text/plain", @_, "", $err, "");
	exit;
}

### Escaping ###

sub url_escape($) {
	my $x = shift @_;
	defined $x or return;
	utf8::encode($x) if $utf8_mode;
	$x =~ s/([^-\$_.!*'(),0-9A-Za-z\x80-\xff])/"%".unpack('H2',$1)/ge;
	utf8::decode($x) if $utf8_mode;
	return $x;
}

sub url_strict_escape($) {
	my $x = shift @_;
	defined $x or return;
	utf8::encode($x);
	$x =~ s/([^-\$_.!*'(),0-9A-Za-z])/"%".unpack('H2',$1)/ge;
	return $x;
}

sub url_deescape($) {
	my $x = shift @_;
	defined $x or return;
	utf8::encode($x) if $utf8_mode;
	$x =~ s/%(..)/pack("H2",$1)/ge;
	utf8::decode($x) if $utf8_mode;
	return $x;
}

sub url_param_escape($) {
	my $x = shift @_;
	defined $x or return;
	utf8::encode($x) if $utf8_mode;
	$x =~ s/([^-\$_.!*'(),0-9A-Za-z])/"%".unpack('H2',$1)/ge;
	$x =~ s/%20/+/g;
	utf8::decode($x) if $utf8_mode;
	return $x;
}

sub url_param_deescape($) {
	my $x = shift @_;
	defined $x or return;
	$x =~ s/\+/ /g;
	return url_deescape($x);
}

sub html_escape($) {
	my $x = shift @_;
	defined $x or return;
	$x =~ s/&/&amp;/g;
	$x =~ s/</&lt;/g;
	$x =~ s/>/&gt;/g;
	$x =~ s/"/&quot;/g;
	$x =~ s/'/&#39;/g;
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
	return url_deescape($x);
}

### Reading of HTTP headers ###

sub http_get($) {
	my $h = shift @_;
	$h =~ tr/a-z-/A-Z_/;
	return $ENV{"HTTP_$h"} // $ENV{"$h"};
}

### Parsing of Arguments ###

my $main_arg_table;
my %raw_args;

sub parse_raw_args_ll($$) {
	my ($arg, $s) = @_;
	$s =~ s/\r\n/\n/g;
	$s =~ s/\r/\n/g;
	utf8::decode($s) if $utf8_mode;
	push @{$raw_args{$arg}}, $s;
}

sub parse_raw_args($) {
	my ($s) = @_;
	$s =~ s/\s+//;
	for $_ (split /[&:]/, $s) {
		(/^([^=]+)=(.*)$/) or next;
		my $arg = $1;
		$_ = $2;
		s/\+/ /g;
		s/%(..)/pack("H2",$1)/eg;
		parse_raw_args_ll($arg, $_);
	}
}

sub parse_multipart_form_data();

sub init_args() {
	if (!defined $ENV{"GATEWAY_INTERFACE"}) {
		print STDERR "Must be called as a CGI script.\n";
		$UCW::CGI::ErrorHandler::exit_code = 1;
		exit;
	}

	my $method = $ENV{"REQUEST_METHOD"};
	if (my $qs = $ENV{"QUERY_STRING"}) {
		parse_raw_args($qs);
	}
	if ($method eq "GET" || $method eq "HEAD") {
	} elsif ($method eq "POST") {
		my $content_type = $ENV{"CONTENT_TYPE"} // "";
		if ($content_type =~ /^application\/x-www-form-urlencoded\b/i) {
			while (<STDIN>) {
				chomp;
				parse_raw_args($_);
			}
		} elsif ($content_type =~ /^multipart\/form-data\b/i) {
			parse_multipart_form_data();
		} else {
			http_error "415 Unsupported Media Type";
			exit;
		}
	} else {
		http_error "405 Method Not Allowed", "Allow: GET, HEAD, POST";
	}
}

sub parse_args($) {			# CAVEAT: attached files must be defined in the main arg table
	my $args = shift @_;
	if (!$main_arg_table) {
		$main_arg_table = $args;
		init_args();
	}

	for my $a (values %$args) {
		my $r = ref($a->{'var'});
		$a->{'default'} //= '';
		if ($r eq 'SCALAR') {
			${$a->{'var'}} = $a->{'default'};
		} elsif ($r eq 'ARRAY') {
			@{$a->{'var'}} = ();
		}
	}

	for my $arg (keys %$args) {
		my $a = $args->{$arg};
		defined($raw_args{$arg}) or next;
		for (@{$raw_args{$arg}}) {
			$a->{'multiline'} or s/(\n|\t)/ /g;
			unless ($a->{'preserve_spaces'}) {
				s/^\s+//;
				s/\s+$//;
			}
			if (my $rx = $a->{'check'}) {
				if (!/^$rx$/) { $_ = $a->{'default'}; }
			}

			my $v = $a->{'var'};
			my $r = ref($v);
			if ($r eq 'SCALAR') {
				$$v = $_;
			} elsif ($r eq 'ARRAY') {
				push @$v, $_;
			}
		}
	}
}

### Parsing Multipart Form Data ###

my $boundary;
my $boundary_len;
my $mp_buffer;
my $mp_buffer_i;
my $mp_buffer_boundary;
my $mp_eof;

sub refill_mp_data($) {
	my ($more) = @_;
	if ($mp_buffer_boundary >= $mp_buffer_i) {
		return $mp_buffer_boundary - $mp_buffer_i;
	} elsif ($mp_buffer_i + $more <= length($mp_buffer) - $boundary_len) {
		return $more;
	} else {
		if ($mp_buffer_i) {
			$mp_buffer = substr($mp_buffer, $mp_buffer_i);
			$mp_buffer_i = 0;
		}
		while ($mp_buffer_i + $more > length($mp_buffer) - $boundary_len) {
			last if $mp_eof;
			my $data;
			my $n = read(STDIN, $data, 2048);
			if ($n > 0) {
				$mp_buffer .= $data;
			} else {
				$mp_eof = 1;
			}
		}
		$mp_buffer_boundary = index($mp_buffer, $boundary, $mp_buffer_i);
		if ($mp_buffer_boundary >= 0) {
			return $mp_buffer_boundary;
		} elsif ($mp_eof) {
			return length($mp_buffer);
		} else {
			return length($mp_buffer) - $boundary_len;
		}
	}
}

sub get_mp_line($) {
	my ($allow_empty) = @_;
	my $n = refill_mp_data(1024);
	my $i = index($mp_buffer, "\r\n", $mp_buffer_i);
	if ($i >= $mp_buffer_i && $i < $mp_buffer_i + $n - 1) {
		my $s = substr($mp_buffer, $mp_buffer_i, $i - $mp_buffer_i);
		$mp_buffer_i = $i + 2;
		return $s;
	} elsif ($allow_empty) {
		if ($n) {							# An incomplete line
			my $s = substr($mp_buffer, $mp_buffer_i, $n);
			$mp_buffer_i += $n;
			return $s;
		} else {							# No more lines
			return undef;
		}
	} else {
		http_error "400 Bad Request: Premature end of multipart POST data";
	}
}

sub skip_mp_boundary() {
	if ($mp_buffer_boundary != $mp_buffer_i) {
		http_error "400 Bad Request: Premature end of multipart POST data";
	}
	$mp_buffer_boundary = -1;
	$mp_buffer_i += 2;
	my $b = get_mp_line(0);
	print STDERR "SEP $b\n" if $debug;
	$mp_buffer_boundary = index($mp_buffer, $boundary, $mp_buffer_i);
	if (substr("\r\n$b", 0, $boundary_len) eq "$boundary--") {
		return 0;
	} else {
		return 1;
	}
}

sub parse_mp_header() {
	my $h = {};
	my $last;
	while ((my $l = get_mp_line(0)) ne "") {
		print STDERR "HH $l\n" if $debug;
		if (my ($name, $value) = ($l =~ /([A-Za-z0-9-]+)\s*:\s*(.*)/)) {
			$name =~ tr/A-Z/a-z/;
			$h->{$name} = $value;
			$last = $name;
		} elsif ($l =~ /^\s+/ && $last) {
			$h->{$last} .= $l;
		} else {
			$last = undef;
		}
	}
	foreach my $n (keys %$h) {
		$h->{$n} = rfc822_prepare($h->{$n});
		print STDERR "H $n: $h->{$n}\n" if $debug;
	}
	return (keys %$h) ? $h : undef;
}

sub parse_multipart_form_data() {
	# First of all, find the boundary string
	my $ct = rfc822_prepare($ENV{"CONTENT_TYPE"});
	if (!(($boundary) = ($ct =~ /^.*;\s*boundary=([^; ]+)/))) {
		http_error "400 Bad Request: Multipart content with no boundary string received";
	}
	$boundary = rfc822_deescape($boundary);
	print STDERR "BOUNDARY IS $boundary\n" if $debug;

	# BUG: IE 3.01 on Macintosh forgets to add the "--" at the start of the boundary string
	# as the MIME specs preach. Workaround borrowed from CGI.pm in Perl distribution.
	my $agent = http_get("User-Agent") // "";
	$boundary = "--$boundary" unless $agent =~ /MSIE\s+3\.0[12];\s*Mac/;
	$boundary = "\r\n$boundary";
	$boundary_len = length($boundary) + 2;

	# Check upload size in advance
	if (my $size = http_get("Content-Length")) {
		my $max_allowed = 0;
		foreach my $a (values %$main_arg_table) {
			$max_allowed += $a->{"maxsize"} || 65536;
		}
		if ($size > $max_allowed) {
			http_error "413 Request Entity Too Large";
		}
	}

	# Initialize our buffering mechanism and part splitter
	$mp_buffer = "\r\n";
	$mp_buffer_i = 0;
	$mp_buffer_boundary = -1;
	$mp_eof = 0;

	# Skip garbage before the 1st part
	while (my $i = refill_mp_data(256)) { $mp_buffer_i += $i; }
	skip_mp_boundary() || return;

	# Process individual parts
	do { PART: {
		print STDERR "NEXT PART\n" if $debug;
		my $h = parse_mp_header();
		my ($field, $cdisp, $a);
		if ($h &&
		    ($cdisp = $h->{"content-disposition"}) &&
		    $cdisp =~ /^form-data/ &&
		    (($field) = ($cdisp =~ /;name=([^;]+)/)) &&
		    ($a = $main_arg_table->{"$field"})) {
			print STDERR "FIELD $field\n" if $debug;
			if (defined $h->{"content-transfer-encoding"}) {
				http_error "400 Bad Request: Unexpected Content-Transfer-Encoding";
			}
			if (defined $a->{"var"}) {
				while (defined (my $l = get_mp_line(1))) {
					print STDERR "VALUE $l\n" if $debug;
					parse_raw_args_ll($field, $l);
				}
				next PART;
			} elsif (defined $a->{"file"}) {
				require File::Temp;
				require IO::Handle;
				my $max_size = $a->{"maxsize"} || 1048576;
				my @tmpargs = (undef, UNLINK => 1);
				push @tmpargs, DIR => $a->{"tmpdir"} if defined $a->{"tmpdir"};
				my ($fh, $fn) = File::Temp::tempfile(@tmpargs);
				print STDERR "FILE UPLOAD to $fn\n" if $debug;
				${$a->{"file"}} = $fn;
				${$a->{"fh"}} = $fh if defined $a->{"fh"};
				if (defined $a->{"filename"}){
					my ($filename) = ($cdisp =~ /;filename=([^;]+)/);
					(${$a->{"filename"}}) = rfc822_deescape($filename) if defined $filename;
				}
				my $total_size = 0;
				while (my $i = refill_mp_data(4096)) {
					print $fh substr($mp_buffer, $mp_buffer_i, $i);
					$mp_buffer_i += $i;
					$total_size += $i;
					if ($total_size > $max_size) { http_error "413 Request Entity Too Large"; }
				}
				$fh->flush();	# Don't close the handle, the file would disappear otherwise
				next PART;
			}
		}
		print STDERR "SKIPPING\n" if $debug;
		while (my $i = refill_mp_data(256)) { $mp_buffer_i += $i; }
	} } while (skip_mp_boundary());
}

### Generating Self-ref URL's ###

sub make_out_args(@) {		# Usage: make_out_args([arg_table, ...] name => value, ...)
	my @arg_tables = ( $main_arg_table );
	while (@_ && ref($_[0]) eq 'HASH') {
		push @arg_tables, shift @_;
	}
	my %overrides = @_;
	my $out = {};
	for my $table (@arg_tables) {
		for my $name (keys %$table) {
			my $arg = $table->{$name};
			defined($arg->{'var'}) || next;
			defined($arg->{'pass'}) && !$arg->{'pass'} && !exists $overrides{$name} && next;
			defined $arg->{'default'} or $arg->{'default'} = "";
			my $value;
			if (!defined($value = $overrides{$name})) {
				if (exists $overrides{$name}) {
					$value = $arg->{'default'};
				} else {
					$value = ${$arg->{'var'}};
					defined $value or $value = $arg->{'default'};
				}
			}
			if ($value ne $arg->{'default'}) {
				$out->{$name} = $value;
			}
		}
	}
	return $out;
}

sub self_ref(@) {
	my $out = make_out_args(@_);
	return "?" . join(':', map { "$_=" . url_param_escape($out->{$_}) } sort keys %$out);
}

sub self_form(@) {
	my $out = make_out_args(@_);
	return join('', map { "<input type=hidden name=$_ value='" . html_escape($out->{$_}) . "'>\n" } sort keys %$out);
}

### Cookies

sub set_cookie($$@) {
	#
	# Unfortunately, the support for the new cookie standard (RFC 2965) among
	# web browsers is still very scarce, so we are still using the old Netscape
	# specification.
	#
	# Usage: set_cookie(name, value, option => value...), where options are:
	#
	#	max-age		maximal age in seconds
	#	domain		domain name scope
	#	path		path name scope
	#	secure		if present, cookie applies only to SSL connections
	#			(in this case, the value should be undefined)
	#	discard		if present with any value, the cookie is discarded
	#

	my $key = shift @_;
	my $value = shift @_;
	my %other = @_;
	if (exists $other{'discard'}) {
		delete $other{'discard'};
		$other{'max-age'} = 0;
	}
	if (defined(my $age = $other{'max-age'})) {
		delete $other{'max-age'};
		my $exp = ($age ? (time + $age) : 0);
		# Avoid problems with locales
		my ($S,$M,$H,$d,$m,$y,$wd) = gmtime $exp;
		my @wdays = ( 'Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat' );
		my @mons = ( 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec' );
		$other{'expires'} = sprintf("%s, %02d-%s-%d %02d:%02d:%02d GMT",
			$wdays[$wd], $d, $mons[$m], $y+1900, $H, $M, $S);
	}

	print "Set-Cookie: $key=", url_strict_escape($value);
	foreach my $k (keys %other) {
		print "; $k";
		print "=", $other{$k} if defined $other{$k};
	}
	print "\n";
}

sub parse_cookies() {
	my $h = http_get("Cookie") or return ();
	my @cook = ();
	foreach my $x (split /;\s*/, $h) {
		my ($k,$v) = split /=/, $x;
		$v = url_deescape($v) if defined $v;
		push @cook, $k => $v;
	}
	return @cook;
}

1;  # OK
