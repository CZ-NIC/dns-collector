#!/usr/bin/perl
# Script for formatting documentation from definition lists
# (they get out of extract-doc.pl as a side-product).
# (c) 2008 Michal Vaner <vorner@ucw.cz>
use strict;
use warnings;

my $head = shift;
my $out = shift;

open OUT, ">$out" or die "Could not write output $out ($!)\n";
open HEAD, $head or die "Could not open head $head ($!)\n";
print OUT foreach( <HEAD> );
close HEAD;

my $dir = $out;
$dir =~ s/\/[^\/]+$//;

while( defined( my $line = <> ) ) {
	chomp $line;
	my( $file, $num, $text ) = split /,/, $line, 3;
	my $dircp = $dir;
	while( shift @{[ $dircp =~ /([^\/]+)/, "//" ]} eq shift @{[ $file =~ /([^\/]+)/, "///" ]} ) {
		$dircp =~ s/[^\/]+\/?//;
		$file =~ s/[^\/]+\/?//;
	}
	$dircp =~ s/[^\/]+/../g;
	$file = $dircp."/".$file;
	$file =~ s/^\///;
	$file =~ s/\.[^.]+$//;
	print OUT "- <<$file:auto_$num,`$text`>>\n";
}

close OUT;
