# UCW Library configuration system: Perl paths
# (c) 2017 Martin Mares <mj@ucw.cz>

package UCW::Configure::Perl;
use UCW::Configure;

use strict;
use warnings;
use Config;

Log "Determining Perl module path ... ";
my $prefix = $Config{installprefix};
$prefix .= '/' unless $prefix =~ m{/$};
my $lib = substr($Config{installvendorlib}, length $prefix);
Set('INSTALL_PERL_DIR', Get('INSTALL_USR_PREFIX') . $lib);
Log Get('INSTALL_PERL_DIR') . "\n";

Log "Determining Perl arch-dependent module path ... ";
my $archlib = substr($Config{installvendorarch}, length $prefix);
Set('INSTALL_PERL_ARCH_DIR', Get('INSTALL_USR_PREFIX') . $archlib);
Log Get('INSTALL_PERL_ARCH_DIR') . "\n";

# We succeeded
1;
