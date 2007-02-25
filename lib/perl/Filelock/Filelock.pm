# Perl module for setting process limits
#
# (c) 2007 Pavel Charvat <pchar@ucw.cz>
#
# This software may be freely distributed and used according to the terms
# of the GNU Lesser General Public License.
#
#
#
# Interface:
#   Sherlock::Filelock::fcntl_lock($fd, $cmd, $type, $whence, $start, $len)
#

package Sherlock::Filelock;

use 5.006;
use strict;
use warnings;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
unshift @DynaLoader::dl_library_path, "lib";

# This allows declaration	use Filelock ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(

) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
);
our $VERSION = '0.01';

bootstrap Sherlock::Filelock $VERSION;

# Preloaded methods go here.

1;
__END__
