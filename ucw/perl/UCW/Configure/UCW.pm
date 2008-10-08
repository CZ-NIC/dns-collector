# Automatic configuration, libucw specific part
# (c) 2008 Michal Vaner <vorner@ucw.cz>
# Code taken from autoconf.cfg by:
# (c) 2005--2008 Martin Mares <mj@ucw.cz>
# (c) 2006 Robert Spalek <robert@ucw.cz>

package UCW::Configure::UCW;
use UCW::Configure;

# Determine page size
Test("CPU_PAGE_SIZE", "Determining page size", sub {
	my $p;
	if (IsSet("CONFIG_DARWIN")) {
		$p = `sysctl -n hw.pagesize`;
		defined $p or Fail "sysctl hw.pagesize failed";
	} elsif (IsSet("CONFIG_LINUX")) {
		$p = `getconf PAGE_SIZE`;
		defined $p or Fail "getconf PAGE_SIZE failed";
	}
	chomp $p;
	return $p;
});

if (IsSet("CONFIG_LARGE_FILES") && IsSet("CONFIG_LINUX")) {
	# Use 64-bit versions of file functions
	Set("CONFIG_LFS");
}

# Decide how will ucw/partmap.c work
Set("PARTMAP_IS_MMAP") if IsSet("CPU_64BIT_POINTERS");

# Option for ucw/mempool.c
Set("POOL_IS_MMAP");

# Guess optimal bit width of the radix-sorter
if (Get("CPU_ARCH") eq "default" || Get("CPU_ARCH") =~ /^i[345]86$/) {
	# This should be safe everywhere
	Set("CONFIG_UCW_RADIX_SORTER_BITS" => 10);
} else {
	# Use this on modern CPU's
	Set("CONFIG_UCW_RADIX_SORTER_BITS" => 12);
}

# If debugging memory allocations:
#LIBS+=-lefence

# Remember PKG_CONFIG_PATH used for building, so that it will be propagated to
# pkg-config's run locally in the makefiles.
Set("PKG_CONFIG_PATH", $ENV{"PKG_CONFIG_PATH"}) if defined $ENV{"PKG_CONFIG_PATH"};

1;
