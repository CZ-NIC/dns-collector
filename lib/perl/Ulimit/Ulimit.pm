package Sherlock::Ulimit;

use 5.006;
use strict;
use warnings;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
unshift @DynaLoader::dl_library_path, "lib";

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

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
