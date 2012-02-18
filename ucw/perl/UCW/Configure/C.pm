# UCW Library configuration system: OS and C compiler
# (c) 2005--2010 Martin Mares <mj@ucw.cz>
# (c) 2006 Robert Spalek <robert@ucw.cz>
# (c) 2008 Michal Vaner <vorner@ucw.cz>

### OS ###

package UCW::Configure::C;
use UCW::Configure;

use strict;
use warnings;

Test("OS", "Checking on which OS we run", sub {
	my $os = `uname`;
	chomp $os;
	Fail "Unable to determine OS type" if $? || $os eq "";
	return $os;
});

if (Get("OS") eq "Linux") {
	Set("CONFIG_LINUX");
} elsif (Get("OS") eq "Darwin") {
	Set("CONFIG_DARWIN");
} else {
	Fail "Don't know how to run on this operating system.";
}

### Compiler ###

# Default compiler
Test("CC", "Checking for C compiler", sub { return "gcc"; });

# GCC version
Test("GCCVER", "Checking for GCC version", sub {
	my $gcc = Get("CC");
	my $ver = `$gcc --version | sed '2,\$d; s/^\\(.* \\)*\\([0-9]*\\.[0-9]*\\).*/\\2/'`;
	chomp $ver;
	Fail "Unable to determine GCC version" if $? || $ver eq "";
	return $ver;
});
my ($gccmaj, $gccmin) = split(/\./, Get("GCCVER"));
my $gccver = 1000*$gccmaj + $gccmin;
$gccver >= 3000 or Fail "GCC older than 3.0 doesn't support C99 well enough.";

### CPU ###

Test("ARCH", "Checking for machine architecture", sub {
	#
	# We have to ask GCC for the target architecture, because it may
	# differ from what uname tells us. This can happen even if we are
	# not cross-compiling, for example on Linux with amd64 kernel, but
	# i386 userspace.
	#
	my $gcc = Get("CC");
	my $mach = `$gcc -dumpmachine 2>/dev/null`;
	if (!$? && $mach ne "") {
		$mach =~ s/-.*//;
	} else {
		$mach = `uname -m`;
		Fail "Unable to determine machine type" if $? || $mach eq "";
	}
	chomp $mach;
	if ($mach =~ /^i[0-9]86$/) {
		return "i386";
	} elsif ($mach =~ /^(x86[_-]|amd)64$/) {
		return "amd64";
	} else {
		return "unknown ($mach)";
	}
});

my $arch = Get("ARCH");
if ($arch eq 'i386') {
	Set("CPU_I386");
	UnSet("CPU_64BIT_POINTERS");
	Set("CPU_LITTLE_ENDIAN");
	UnSet("CPU_BIG_ENDIAN");
	Set("CPU_ALLOW_UNALIGNED");
	Set("CPU_STRUCT_ALIGN" => 4);
} elsif ($arch eq "amd64") {
	Set("CPU_AMD64");
	Set("CPU_64BIT_POINTERS");
	Set("CPU_LITTLE_ENDIAN");
	UnSet("CPU_BIG_ENDIAN");
	Set("CPU_ALLOW_UNALIGNED");
	Set("CPU_STRUCT_ALIGN" => 8);
} elsif (!Get("CPU_LITTLE_ENDIAN") && !Get("CPU_BIG_ENDIAN")) {
	Fail "Architecture not recognized, please set CPU_xxx variables manually.";
}

### Compiler and its Options ###

# C flags: tell the compiler we're speaking C99, and disable common symbols
Set("CLANG" => "-std=gnu99 -fno-common");

# C optimizations
Set("COPT" => '-O2');
if ($arch =~ /^(i386|amd64)$/ && Get("CONFIG_EXACT_CPU")) {
	if ($gccver >= 4002) {
		Append('COPT', '-march=native');
	} else {
		Warn "CONFIG_EXACT_CPU not supported with old GCC, ignoring.\n";
	}
}

# C optimizations for highly exposed code
Set("COPT2" => '-O3');

# Warnings
Set("CWARNS" => '-Wall -W -Wno-parentheses -Wstrict-prototypes -Wmissing-prototypes -Winline');
Set("CWARNS_OFF" => '');

# Linker flags
Set("LOPT" => "");

# Extra libraries
Set("LIBS" => "");

# Extra flags for compiling and linking shared libraries
Set("CSHARED" => '-fPIC');
if (IsSet("CONFIG_LOCAL")) {
	Set("SONAME_PREFIX" => "lib/");
} else {
	Set("SONAME_PREFIX" => "");
}
if (IsSet("CONFIG_DARWIN")) {
	Set("LSHARED" => '-dynamiclib -install_name $(SONAME_PREFIX)$(@F)$(SONAME_SUFFIX) -undefined dynamic_lookup');
} else {
	Set("LSHARED" => '-shared -Wl,-soname,$(SONAME_PREFIX)$(@F)$(SONAME_SUFFIX)');
}

# Extra switches depending on GCC version:
if ($gccver == 3000) {
	Append("COPT" => "-fstrict-aliasing");
} elsif ($gccver == 3003) {
	Append("CWARNS" => "-Wundef -Wredundant-decls");
	Append("COPT" => "-finline-limit=20000 --param max-inline-insns-auto=1000");
} elsif ($gccver == 3004) {
	Append("CWARNS" => "-Wundef -Wredundant-decls");
	Append("COPT" => "-finline-limit=2000 --param large-function-insns=5000 --param inline-unit-growth=200 --param large-function-growth=400");
} elsif ($gccver >= 4000) {
	Append("CWARNS" => "-Wundef -Wredundant-decls -Wno-pointer-sign -Wdisabled-optimization -Wno-missing-field-initializers");
	Append("CWARNS_OFF" => "-Wno-pointer-sign");
	Append("COPT" => "-finline-limit=5000 --param large-function-insns=5000 --param inline-unit-growth=200 --param large-function-growth=400");
	if ($gccver >= 4002) {
		Append("COPT" => "-fgnu89-inline");
	}
} else {
	Warn "Don't know anything about this GCC version, using default switches.\n";
}

if (IsSet("CONFIG_DEBUG")) {
	# If debugging:
	Set("DEBUG_ASSERTS");
	Set("DEBUG_DIE_BY_ABORT") if Get("CONFIG_DEBUG") > 1;
	Set("CDEBUG" => "-ggdb");
} else {
	# If building a release version:
	Append("COPT" => "-fomit-frame-pointer");
	Append("LOPT" => "-s");
}

if (IsSet("CONFIG_DARWIN")) {
	# gcc-4.0 on Darwin doesn't set this in the gnu99 mode
	Append("CLANG" => "-fnested-functions");
	# Directory hierarchy of the fink project
	Append("LIBS" => "-L/sw/lib");
	Append("COPT" => "-I/sw/include");
}

### Compiling test programs ###

sub TestCompile($$) {
	my ($testname, $source) = @_;
	my $dir = "conftest-$testname";
	`rm -rf $dir && mkdir $dir`; $? and Fail "Cannot initialize $dir";

	open SRC, ">$dir/conftest.c";
	print SRC $source;
	close SRC;

	my $cmd = join(" ",
		map { defined($_) ? $_ : "" }
			"cd $dir &&",
			Get("CC"), Get("CLANG"), Get("COPT"), Get("CEXTRA"), Get("LIBS"),
			"conftest.c", "-o", "conftest",
			">conftest.log", "2>&1"
		);
	`$cmd`;
	my $result = !$?;

	`rm -rf $dir` unless Get("KEEP_CONFTEST");

	return $result;
}

### Writing C headers with configuration ###

sub ConfigHeader($$) {
	my ($hdr, $rules) = @_;
	Log "Generating $hdr ... ";
	open X, ">obj/$hdr" or Fail $!;
	print X "/* Generated automatically by $0, please don't touch manually. */\n";

	sub match_rules($$) {
		my ($rules, $name) = @_;
		for (my $i=0; $i < scalar @$rules; $i++) {
			my ($r, $v) = ($rules->[$i], $rules->[$i+1]);
			return $v if $name =~ $r;
		}
		return 0;
	}

	foreach my $x (sort keys %UCW::Configure::vars) {
		next unless match_rules($rules, $x);
		my $v = $UCW::Configure::vars{$x};
		# Try to add quotes if necessary
		$v = '"' . $v . '"' unless ($v =~ /^"/ || $v =~ /^\d*$/);
		print X "#define $x $v\n";
	}
	close X;
	Log "done\n";
}

AtWrite {
	ConfigHeader("autoconf.h", [
		# Symbols with "_" anywhere in their name are exported
		"_" => 1
	]);
};

# Return success
1;
