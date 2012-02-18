# UCW Library configuration system: parameters of the library
# (c) 2005--2010 Martin Mares <mj@ucw.cz>
# (c) 2006 Robert Spalek <robert@ucw.cz>
# (c) 2008 Michal Vaner <vorner@ucw.cz>

package UCW::Configure::LibUCW;
use UCW::Configure;

use strict;
use warnings;

# Turn on debugging support if CONFIG_DEBUG
if (Get("CONFIG_DEBUG")) {
	Set("CONFIG_UCW_DEBUG");
}

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

# Decide how will ucw/partmap.c work
Set("CONFIG_UCW_PARTMAP_IS_MMAP") if IsSet("CPU_64BIT_POINTERS");

# Option for ucw/mempool.c
Set("CONFIG_UCW_POOL_IS_MMAP");

# Guess optimal bit width of the radix-sorter
Test("CONFIG_UCW_RADIX_SORTER_BITS", "Determining radix sorter bucket width", sub {
	if (Get("CPU_AMD64")) {
		# All amd64 CPUs have large enough L1 cache
		return 12;
	} else {
		# This should be safe everywhere
		return 10;
	}
});

# Detect if thread-local storage is supported
if (Get("CONFIG_UCW_THREADS")) {
	TestBool("CONFIG_UCW_TLS", "Checking if GCC supports thread-local storage", sub {
		if (UCW::Configure::C::TestCompile("__thread", "__thread int i;\nint main(void) { return 0; }\n")) {
			return 1;
		} else {
			return 0;
		}
	});
}

# Detect if we have the epoll() syscall
TestBool("CONFIG_UCW_EPOLL", "Checking for epoll", sub {
	return UCW::Configure::C::TestCompile("epoll", <<'FINIS' ) ? 1 : 0;
#include <sys/epoll.h>
int main(void)
{
	epoll_create(256);
	return 0;
}
FINIS
});

# Check if we want to use monotonic clock
TestBool("CONFIG_UCW_MONOTONIC_CLOCK", "Checking for monotonic clock", sub {
	return Get("CONFIG_LINUX");
});

if (IsSet("CONFIG_DARWIN")) {
	# Darwin does not support BSD regexes, fix up
	if (!IsSet("CONFIG_UCW_POSIX_REGEX") && !IsSet("CONFIG_UCW_PCRE")) {
		Set("CONFIG_UCW_POSIX_REGEX" => 1);
		Warn "BSD regex library on Darwin isn't compatible, using POSIX regex.\n";
	}

	# Fill in some constants not found in the system header files
	Set("SOL_TCP" => 6);		# missing in /usr/include/netinet/tcp.h
	if (IsGiven("CONFIG_UCW_DIRECT_IO") && IsSet("CONFIG_UCW_DIRECT_IO")) {
		Fail("Direct I/O is not available on darwin");
	} else {
		UnSet("CONFIG_UCW_DIRECT_IO");
	}
}

PostConfig {
	AtWrite {
		UCW::Configure::C::ConfigHeader("ucw/autoconf.h", [
			# Included symbols
			'^CONFIG_UCW_' => 1,
			'^CPU_' => 1,
			'^(SHERLOCK|UCW)_VERSION(_|$)' => 1,

		]);
	} if Get("CONFIG_INSTALL_API");

	# Include direct FB?
	if (!IsSet("CONFIG_UCW_THREADS") || !IsSet("CONFIG_UCW_DIRECT_IO")) {
		if (IsGiven("CONFIG_UCW_FB_DIRECT") && IsSet("CONFIG_UCW_FB_DIRECT")) {
			if (!IsSet("CONFIG_UCW_THREADS")) {
				Fail("CONFIG_UCW_FB_DIRECT needs CONFIG_UCW_THREADS");
			} else {
				Fail("CONFIG_UCW_FB_DIRECT needs CONFIG_UCW_DIRECT_IO");
			}
		}
		UnSet("CONFIG_UCW_FB_DIRECT");
	}
};

# We succeeded
1;
