#	Perl module for UCW Configure Scripts
#
#	(c) 2005 Martin Mares <mj@ucw.cz>
#
#	This software may be freely distributed and used according to the terms
#	of the GNU Lesser General Public License.

package UCW::Configure;

use strict;
use warnings;

BEGIN {
	# The somewhat hairy Perl export mechanism
	use Exporter();
	our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
	$VERSION = 1.0;
	@ISA = qw(Exporter);
	@EXPORT = qw(&Init &Log &Notice &Fail &IsSet &Set &UnSet &Append &Override &Get &Test &Include &Finish &FindFile &TryFindFile);
	@EXPORT_OK = qw();
	%EXPORT_TAGS = ();
}

our %vars = ();
our %overriden = ();

sub Log($) {
	print @_;
}

sub Notice($) {
	print @_ if $vars{"VERBOSE"};
}

sub Fail($) {
	Log((shift @_) . "\n");
	exit 1;
}

sub IsSet($) {
	my ($x) = @_;
	return exists $vars{$x};
}

sub Get($) {
	my ($x) = @_;
	return $vars{$x};
}

sub Set($;$) {
	my ($x,$y) = @_;
	$y=1 unless defined $y;
	$vars{$x}=$y unless $overriden{$x};
}

sub UnSet($) {
	my ($x) = @_;
	delete $vars{$x} unless $overriden{$x};
}

sub Append($$) {
	my ($x,$y) = @_;
	Set($x, (IsSet($x) ? (Get($x) . " $y") : $y));
}

sub Override($;$) {
	my ($x,$y) = @_;
	$y=1 unless defined $y;
	$vars{$x}=$y;
	$overriden{$x} = 1;
}

sub Test($$$) {
	my ($var,$msg,$sub) = @_;
	Log "$msg ... ";
	if (!IsSet($var)) {
		Set $var, &$sub();
	}
	Log Get($var) . "\n";
}

sub TryFindFile($) {
	my ($f) = @_;
	if (-f $f) {
		return $f;
	} elsif ($f !~ /^\// && -f (Get("SRCDIR")."/$f")) {
		return Get("SRCDIR")."/$f";
	} else {
		return undef;
	}
}

sub FindFile($) {
	my ($f) = @_;
	my $F;
	defined ($F = TryFindFile($f)) or Fail "Cannot find file $f";
	return $F;
}

sub Init($$) {
	my ($srcdir,$defconfig) = @_;
	if ((!defined $defconfig && !@ARGV) || @ARGV && $ARGV[0] eq "--help") {
		print STDERR "Usage: [<srcdir>/]configure " . (defined $defconfig ? "[" : "") . "<config-name>" . (defined $defconfig ? "]" : "") .
			" [<option>[=<value>] | -<option>] ...\n";
		exit 1;
	}
	if (@ARGV && $ARGV[0] !~ /=/) {
		Set('CONFIG' => shift @ARGV);
	} else {
		Set('CONFIG' => $defconfig);
	}
	Set("SRCDIR", $srcdir);

	foreach my $x (@ARGV) {
		if ($x =~ /^(\w+)=(.*)/) {
			Override($1 => $2);
		} elsif ($x =~ /^-(\w+)$/) {
			Override($1 => 1);
			delete $vars{$1};
		} elsif ($x =~ /^(\w+)$/) {
			Override($1 => 1);
		} else {
			print STDERR "Invalid option $_\n";
			exit 1;
		}
	}

	if (!TryFindFile(Get("CONFIG"))) {
		TryFindFile(Get("CONFIG")."/config") or Fail "Cannot find configuration " . Get("CONFIG");
		Override("CONFIG" => Get("CONFIG")."/config");
	}
}

sub Include($) {
	my ($f) = @_;
	$f = FindFile($f);
	Notice "Loading configuration $f\n";
	require $f;
}

sub Finish() {
	print "\n";

	if (Get("SRCDIR") ne ".") {
		Log "Preparing for compilation from directory " . Get("SRCDIR") . " to obj/ ... ";
		-l "src" and unlink "src";
		symlink Get("SRCDIR"), "src" or Fail "Cannot link source directory to src: $!";
		Override("SRCDIR" => "src");
		-l "Makefile" and unlink "Makefile";
		-f "Makefile" and Fail "Makefile already exists";
		symlink "src/Makefile", "Makefile" or Fail "Cannot link Makefile: $!";
	} else {
		Log "Preparing for compilation from current directory to obj/ ... ";
	}
	`rm -rf obj` if -d "obj"; Fail "Cannot delete old obj directory" if $?;
	-d "obj" or mkdir("obj", 0777) or Fail "Cannot create obj directory: $!";
	-d "obj/lib" or mkdir("obj/lib", 0777) or Fail "Cannot create obj/lib directory: $!";
	Log "done\n";

	Log "Generating autoconf.h ... ";
	open X, ">obj/lib/autoconf.h" or Fail $!;
	print X "/* Generated automatically by $0, please don't touch manually. */\n";
	foreach my $x (sort keys %vars) {
		# Don't export variables which contain no underscores
		next unless $x =~ /_/;
		my $v = $vars{$x};
		# Try to add quotes if necessary
		$v = '"' . $v . '"' unless ($v =~ /^"/ || $v =~ /^\d*$/);
		print X "#define $x $v\n";
	}
	close X;
	Log "done\n";

	Log "Generating config.mk ... ";
	open X, ">obj/config.mk" or Fail $!;
	print X "# Generated automatically by $0, please don't touch manually.\n";
	foreach my $x (sort keys %vars) {
		print X "$x=$vars{$x}\n";
	}
	close X;
	Log "done\n";
}

1;  # OK
