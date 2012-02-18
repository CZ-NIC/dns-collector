#!/usr/bin/perl
# A simple system for making software releases
# (c) 2003--2012 Martin Mares <mj@ucw.cz>

package UCW::Release;
use strict;
use warnings;
use Getopt::Long;

our $verbose = 0;

sub new($$) {
	my ($class,$basename) = @_;
	my $s = {
		"PACKAGE" => $basename,
		"rules" => [
			# p=preprocess, s=subst, -=discard
			'(^|/)(CVS|\.arch-ids|{arch}|\.git|tmp)/' => '-',
			'\.(lsm|spec)$' => 'ps',
			'(^|/)README$' => 's'
			],
		"conditions" => {
			# Symbols, which can serve as conditions for the preprocessor
			},
		"DATE" => `date '+%Y-%m-%d' | tr -d '\n'`,
		"LSMDATE" => `date '+%y%m%d' | tr -d '\n'`,
		"distfiles" => [
			# Files to be uploaded
			],
		"uploads" => [
			# Locations where we want to upload, e.g.:
			#	{ "url" => "ftp://metalab.unc.edu/incoming/linux/",
			#	  "filter" => '(\.tar\.gz|\.lsm)$', }
			],
		"test_compile" => "make",
		# "archive_dir" => "/tmp/archives/$basename",
		# Options
		"do_test" => 1,
		"do_patch" => 0,
		"diff_against" => "",
		"do_upload" => 1,
		"do_sign" => 1,
	};
	bless $s;
	return $s;
}

sub Confirm($) {
	my ($s) = @_;
	print "<confirm> "; <STDIN>;
}

sub GetVersionFromGit($) {
	my ($s) = @_;
	return if defined $s->{"VERSION"};
	my $desc = `git describe --tags`; die "git describe failed\n" if $?;
	chomp $desc;
	my ($ver, $rest) = ($desc =~ /^v([0-9.]+)(.*)/) or die "Failed to understand output of git describe: $desc\n";
	print "Detected version $ver from git description $desc\n";
	if ($rest ne '') {
		print "WARNING: We are several commits past release tag... ";
		$s->Confirm;
	}
	$s->{"VERSION"} = $ver;
	return $ver;
}

sub GetVersionFromFile($) {
	my ($s,$file,$rx) = @_;
	return if defined $s->{"VERSION"};
	open F, $file or die "Unable to open $file for version autodetection";
	while (<F>) {
		chomp;
		if (/$rx/) {
			$s->{"VERSION"} = $1;
			print "Detected version $1 from $file\n" if $verbose;
			last;
		}
	}
	close F;
	if (!defined $s->{"VERSION"}) { die "Failed to auto-detect version"; }
	return $s->{"VERSION"};
}

sub GetVersionsFromChangelog($) {
	my ($s,$file,$rx) = @_;
	return if defined $s->{"VERSION"};
	open F, $file or die "Unable to open $file for version autodetection";
	while (<F>) {
		chomp;
		if (/$rx/) {
			if (!defined $s->{"VERSION"}) {
				$s->{"VERSION"} = $1;
				print "Detected version $1 from $file\n" if $verbose;
			} elsif ($s->{"VERSION"} eq $1) {
				# do nothing
			} else {
				$s->{"OLDVERSION"} = $1;
				print "Detected previous version $1 from $file\n" if $verbose;
				last;
			}
		}
	}
	close F;
	if (!defined $s->{"VERSION"}) { die "Failed to auto-detect version"; }
	return $s->{"VERSION"};
}

sub InitDist($) {
	my ($s,$dd) = @_;
	$s->{"DISTDIR"} = $dd;
	print "Initializing dist directory $dd\n" if $verbose;
	`rm -rf $dd`; die if $?;
	`mkdir -p $dd`; die if $?;

	if ($s->{"archive_dir"}) {
		unshift @{$s->{"uploads"}}, { "url" => "file:" . $s->{"archive_dir"} };
	}
}

sub ExpandVar($$) {
	my ($s,$v) = @_;
	if (defined $s->{$v}) {
		return $s->{$v};
	} else {
		die "Reference to unknown variable $v";
	}
}

sub TransformFile($$$) {
	my ($s,$file,$action) = @_;

	my $preprocess = ($action =~ /p/);
	my $subst = ($action =~ /s/);
	my $dest = "$file.dist";
	open I, "<", $file or die "open($file): $?";
	open O, ">", "$dest" or die "open($dest): $!";
	my @ifs = ();	# stack of conditions, 1=satisfied
	my $empty = 0;	# last line was empty
	my $is_makefile = ($file =~ /(Makefile|.mk)$/);
	while (<I>) {
		if ($subst) {
			s/@([0-9A-Za-z_]+)@/$s->ExpandVar($1)/ge;
		}
		if ($preprocess) {
			if (/^#/ || $is_makefile) {
				if (/^#?ifdef\s+(\w+)/) {
					if (defined ${$s->{"conditions"}}{$1}) {
						push @ifs, ${$s->{"conditions"}}{$1};
						next;
					}
					push @ifs, 0;
				} elsif (/^#ifndef\s+(\w+)/) {
					if (defined ${$s->{"conditions"}}{$1}) {
						push @ifs, -${$s->{"conditions"}}{$1};
						next;
					}
					push @ifs, 0;
				} elsif (/^#if\s+/) {
					push @ifs, 0;
				} elsif (/^#?endif/) {
					my $x = pop @ifs;
					defined $x or die "Improper nesting of conditionals";
					$x && next;
				} elsif (/^#?else/) {
					my $x = pop @ifs;
					defined $x or die "Improper nesting of conditionals";
					push @ifs, -$x;
					$x && next;
				}
			}
			@ifs && $ifs[$#ifs] < 0 && next;
			if (/^$/) {
				$empty && next;
				$empty = 1;
			} else { $empty = 0; }
		}
		print O;
	}
	close O;
	close I;
	! -x $file or chmod(0755, "$dest") or die "chmod($dest): $!";
	rename $dest, $file or "rename($dest,$file): $!";
}

sub GenPackage($) {
	my ($s) = @_;
	$s->{"PKG"} = $s->{"PACKAGE"} . "-" . $s->{"VERSION"};
	my $dd = $s->{"DISTDIR"};
	my $pkg = $s->{"PKG"};
	my $dir = "$dd/$pkg";
	print "Generating $dir\n";

	system "git archive --format=tar --prefix=$dir/ HEAD | tar xf -";
	die if $?;

	my @files = `cd $dir && find . -type f`;
	die if $?;

	for my $f (@files) {
		chomp $f;
		$f =~ s/^\.\///;
		my $action = "";
		my @rules = @{$s->{"rules"}};
		while (@rules) {
			my $rule = shift @rules;
			my $act = shift @rules;
			if ($f =~ $rule) {
				$action = $act;
				last;
			}
		}
		if ($action eq '') {
		} elsif ($action =~ /-/) {
			unlink "$dir/$f" or die "Cannot unlink $dir/$f: $!\n";
			print "$f (unlinked)\n" if $verbose;
		} else {
			print "$f ($action)\n" if $verbose;
			$s->TransformFile("$dir/$f", $action);
		}
	}

	return $dir;
}

sub GenFile($$) {
	my ($s,$f) = @_;
	my $sf = $s->{"DISTDIR"} . "/" . $s->{"PKG"} . "/$f";
	my $df = $s->{"DISTDIR"} . "/$f";
	print "Generating $df\n";
	`cp $sf $df`; die if $?;
	push @{$s->{"distfiles"}}, $df;
}

sub SignFile($$) {
	my ($s, $file) = @_;
	$s->{'do_sign'} or return;
	print "Signing $file\n";
	system "gpg", "--armor", "--detach-sig", "$file";
	die if $?;
	rename "$file.asc", "$file.sign" or die "No signature produced!?\n";
	push @{$s->{"distfiles"}}, "$file.sign";
}

sub MakeArchive($) {
	my ($s) = @_;
	my $dd = $s->{"DISTDIR"};
	my $pkg = $s->{"PKG"};

	print "Creating $dd/$pkg.tar\n";
	my $tarvv = $verbose ? "vv" : "";
	`cd $dd && tar c${tarvv}f $pkg.tar $pkg >&2`; die if $?;

	print "Creating $dd/$pkg.tar.gz\n";
	`gzip <$dd/$pkg.tar >$dd/$pkg.tar.gz`; die if $?;
	push @{$s->{"distfiles"}}, "$dd/$pkg.tar.gz";

	# print "Creating $dd/$pkg.tar.bz2\n";
	# `bzip2 <$dd/$pkg.tar >$dd/$pkg.tar.bz2`; die if $?;
	# push @{$s->{"distfiles"}}, "$dd/$pkg.tar.bz2";

	$s->SignFile("$dd/$pkg.tar");
}

sub ParseOptions($) {
	my ($s) = @_;
	GetOptions(
		"verbose!" => \$verbose,
		"test!" => \$s->{"do_test"},
		"patch!" => \$s->{"do_patch"},
		"diff-against=s" => \$s->{"diff_against"},
		"version=s" => \$s->{"VERSION"},
		"upload!" => \$s->{"do_upload"},
		"sign!" => \$s->{"do_sign"},
	) || die "Syntax: release [--verbose] [--test] [--nopatch] [--version=<version>] [--diff-against=<version>] [--noupload] [--nosign]";
}

sub Test($) {
	my ($s) = @_;
	$s->{"do_test"} or return;
	my $dd = $s->{"DISTDIR"};
	my $pkg = $s->{"PKG"};
	my $tdir = "$dd/$pkg.test";
	$s->{"TESTDIR"} = $tdir;
	`cp -a $dd/$pkg $tdir`; die if $?;
	my $log = "$tdir.log";
	print "Doing a test compilation\n";
	my $make = $s->{"test_compile"};
	`( cd $tdir && $make ) >$log 2>&1`;
	die "There were errors. Please inspect $log" if $?;
	`grep -q [Ww]arning $log`;
	$? or print "There were warnings! Please inspect $log.\n";
}

sub MakePatch($) {
	my ($s) = @_;
	$s->{"do_patch"} or return;
	my $dd = $s->{"DISTDIR"};
	my $pkg1 = $s->{"PKG"};
	my $oldver;
	if ($s->{"diff_against"} ne "") {
		$oldver = $s->{"diff_against"};
	} elsif (defined $s->{"OLDVERSION"}) {
		$oldver = $s->{"OLDVERSION"};
	} else {
		print "WARNING: No previous version known. No patch generated.\n";
		return;
	}
	my $pkg0 = $s->{"PACKAGE"} . "-" . $oldver;

	my $oldarch = $s->{"archivedir"} . "/" . $pkg0 . ".tar.gz";
	-f $oldarch or die "MakePatch: $oldarch not found";
	print "Unpacking $pkg0 from $oldarch\n";
	`cd $dd && tar xzf $oldarch`; die if $?;

	my $diff = $s->{"PACKAGE"} . "-" . $oldver . "-" . $s->{"VERSION"} . ".diff.gz";
	print "Creating a patch from $pkg0 to $pkg1: $diff\n";
	`cd $dd && diff -ruN $pkg0 $pkg1 | gzip >$diff`; die if $?;
	push @{$s->{"distfiles"}}, "$dd/$diff";
	$s->SignFile("$dd/$diff");
}

sub Upload($) {
	my ($s) = @_;
	foreach my $u (@{$s->{"uploads"}}) {
		my $url = $u->{"url"};
		print "Upload to $url :\n";
		my @files = ();
		my $filter = $u->{"filter"} || ".*";
		foreach my $f (@{$s->{"distfiles"}}) {
			if ($f =~ $filter) {
				print "\t$f\n";
				push @files, $f;
			}
		}
		$s->Confirm;
		if ($url =~ m@^file:(.*)@) {
			my $dir = $1;
			$dir =~ s@^///@/@;
			`cp @files $dir/`; die if $?;
		} elsif ($url =~ m@^scp://([^/]+)(.*)@) {
			$, = " ";
			my $host = $1;
			my $dir = $2;
			$dir =~ s@^/~@~@;
			$dir =~ s@^/\./@@;
			my $cmd = "scp @files $host:$dir\n";
			`$cmd`; die if $?;
		} elsif ($url =~ m@ftp://([^/]+)(.*)@) {
			my $host = $1;
			my $dir = $2;
			open FTP, "|ftp -v $host" or die;
			print FTP "cd $dir\n";
			foreach my $f (@files) {
				(my $ff = $f) =~ s@.*\/([^/].*)@$1@;
				print FTP "put $f $ff\n";
			}
			print FTP "bye\n";
			close FTP;
			die if $?;
		} else {
			die "Don't know how to handle this URL scheme";
		}
	}
}

1;
