#
#	Perl module for Logging
#
#	(c) 2007 Pavel Charvat <pchar@ucw.cz>
#

package Sherlock::Log;

use lib 'lib/perl5';
use strict;
use warnings;
use POSIX;

my $Prog = (reverse split(/\//, $0))[0];

sub Log {
  my $level = shift;
  my $text = join(' ', @_);
  print STDERR $level, strftime(" %Y-%m-%d %H:%M:%S ", localtime()), "[$Prog] ", $text, "\n";
}

sub Die {
  Log('!', @_);
  exit 1;
}

1;
