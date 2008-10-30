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
	@EXPORT = qw(&Init &Log &Notice &Warn &Fail &IsSet &IsGiven &Set &UnSet &Append &Override &Get &Test &Include &Finish &FindFile &TryFindFile &TryCmd &PkgConfig &TrivConfig &debPrint);
	@EXPORT_OK = qw();
	%EXPORT_TAGS = ();
}

our %vars;
our %overriden;
our @postconfigs;

sub debPrint() {
  print "VARS:\n";
#  print "$_: $vars{$_}\n" foreach( keys %vars );
}

sub Log($) {
	print @_;
}

sub Notice($) {
	print @_ if $vars{"VERBOSE"};
}

sub Warn($) {
	print "WARNING: ", @_;
}

sub Fail($) {
	Log("ERROR: " . (shift @_) . "\n");
	exit 1;
}

sub IsSet($) {
	my ($x) = @_;
	return exists $vars{$x};
}

sub IsGiven($) {
	my ($x) = @_;
	return exists $overriden{$x};
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
	sub usage($) {
		my ($dc) = @_;
		print STDERR "Usage: [<srcdir>/]configure " . (defined $dc ? "[" : "") . "<config-name>" . (defined $dc ? "]" : "") .
			" [<option>[=<value>] | -<option>] ...\n";
		exit 1;
	}
	Set('CONFIG' => $defconfig) if defined $defconfig;
	if (@ARGV) {
		usage($defconfig) if $ARGV[0] eq "--help";
		if (!defined($defconfig) || $ARGV[0] !~ /^-?[A-Z][A-Z0-9_]*(=|$)/) {
			# This does not look like an option, so read it as a file name
			Set('CONFIG' => shift @ARGV);
		}
	}
	Set("SRCDIR", $srcdir);

	foreach my $x (@ARGV) {
		if ($x =~ /^(\w+)=(.*)/) {
			Override($1 => $2);
		} elsif ($x =~ /^-(\w+)$/) {
			Override($1 => 0);
			delete $vars{$1};
		} elsif ($x =~ /^(\w+)$/) {
			Override($1 => 1);
		} else {
			print STDERR "Invalid option $x\n";
			exit 1;
		}
	}

	defined Get("CONFIG") or usage($defconfig);
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

sub PostConfig(&) {
	unshift @postconfigs, $_[0];
}

sub Finish() {
	for my $post (@postconfigs) {
		&$post();
	}

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
	if (-d "obj") {
		`rm -rf obj`; Fail "Cannot delete old obj directory" if $?;
	}
	-d "obj" or mkdir("obj", 0777) or Fail "Cannot create obj directory: $!";
	-d "obj/ucw" or mkdir("obj/ucw", 0777) or Fail "Cannot create obj/ucw directory: $!";
	Log "done\n";

	Log "Generating autoconf.h ... ";
	open X, ">obj/autoconf.h" or Fail $!;
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
	print X "s=\${SRCDIR}\n";
	print X "o=obj\n";
	close X;
	Log "done\n";
}

sub TryCmd($) {
	my ($cmd) = @_;
	my $res = `$cmd`;
	defined $res or return;
	chomp $res;
	return $res unless $?;
	return;
}

sub maybe_manually($) {
	my ($n) = @_;
	if (IsGiven($n)) {
		if (Get("$n")) { Log "YES (set manually)\n"; }
		else { Log "NO (set manually)\n"; }
		return 1;
	}
	return 0;
}

sub PkgConfig($@) {
	my $pkg = shift @_;
	my %opts = @_;
	my $upper = $pkg; $upper =~ tr/a-z/A-Z/; $upper =~ s/[^0-9A-Z]+/_/g;
	Log "Checking for package $pkg ... ";
	maybe_manually("CONFIG_HAVE_$upper") and return Get("CONFIG_HAVE_$upper");
	my $ver = TryCmd("pkg-config --modversion $pkg 2>/dev/null");
	if (!defined $ver) {
		Log("NONE\n");
		return 0;
	}
	if (defined($opts{minversion})) {
		my $min = $opts{minversion};
		if (!defined TryCmd("pkg-config --atleast-version=$min $pkg")) {
			Log("NO: version $ver is too old (need >= $min)\n");
			return 0;
		}
	}
	Log("YES: version $ver\n");
	Set("CONFIG_HAVE_$upper" => 1);
	Set("CONFIG_VER_$upper" => $ver);
	my $cf = TryCmd("pkg-config --cflags $pkg");
	Set("${upper}_CFLAGS" => $cf) if defined $cf;
	my $lf = TryCmd("pkg-config --libs $pkg");
	Set("${upper}_LIBS" => $lf) if defined $lf;
	return 1;
}

sub ver_norm($) {
	my ($v) = @_;
	return join(".", map { sprintf("%05s", $_) } split(/\./, $v));
}

sub TrivConfig($@) {
	my $pkg = shift @_;
	my %opts = @_;
	my $upper = $pkg; $upper =~ tr/a-z/A-Z/; $upper =~ s/[^0-9A-Z]+/_/g;
	Log "Checking for package $pkg ... ";
	maybe_manually("CONFIG_HAVE_$upper") and return Get("CONFIG_HAVE_$upper");
	my $pc = $opts{script};
	my $ver = TryCmd("$pc --version 2>/dev/null");
	if (!defined $ver) {
		Log("NONE\n");
		return 0;
	}
	if (defined($opts{minversion})) {
		my $min = $opts{minversion};
		if (ver_norm($ver) lt ver_norm($min)) {
			Log("NO: version $ver is too old (need >= $min)\n");
			return 0;
		}
	}
	Log("YES: version $ver\n");
	Set("CONFIG_HAVE_$upper" => 1);
	Set("CONFIG_VER_$upper" => $ver);

	my $want = $opts{want};
	defined $want or $want = ["cflags", "libs"];
	for my $w (@$want) {
		my $uw = $w; $uw =~ tr/a-z-/A-Z_/;
		my $cf = TryCmd("$pc --$w");
		Set("${upper}_${uw}" => $cf) if defined $cf;
	}
	return 1;
}

1;  # OK
