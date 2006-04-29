# Perl module for setting process limits
#
# (c) 2003 Tomas Valla <tom@ucw.cz>
#
# This software may be freely distributed and used according to the terms
# of the GNU Lesser General Public License.
#
#
#
# Interface:
#   Sherlock::Ulimit::setlimit( $resource, $softlimit, $hardlimit)
#   Sherlock::Ulimit::getlimit( $resource, $softlimit, $hardlimit)
#
# setlimit sets limit to values supplied in softlimit and hardlimit
# getlimit reads limits into softlimit and hardlimit
# $resource constants are defined below
#

package Sherlock::Ulimit;

use 5.006;
use strict;
use warnings;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
unshift @DynaLoader::dl_library_path, "lib";

our $CPU = 0;
our $FSIZE = 1;
our $DATA = 2;
our $STACK = 3;
our $CORE = 4;
our $RSS = 5;
our $NPROC = 6;
our $NOFILE = 7;
our $MEMLOCK = 8;
our $AS = 9;


# This allows declaration	use Ulimit ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(

) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
);
our $VERSION = '0.01';

bootstrap Sherlock::Ulimit $VERSION;

# Preloaded methods go here.

1;
__END__
