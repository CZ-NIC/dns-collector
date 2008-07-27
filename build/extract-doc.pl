#!/usr/bin/perl
use strict;
use warnings;

my( $inname, $outname, $depname, $basedir ) = @ARGV;
if( defined $inname ) {
	open IN, $inname or die "Could not read $inname ($!)\n";
} else {
	open IN, "<&STDIN" or die "Could not read stdin ($!)\n";
}
if( defined $outname ) {
	open OUT, ">$outname" or die "Could not write $outname ($!)\n";
} else {
	open OUT, ">&STDOUT" or die "Could not write to stdout ($!)\n";
}
my $hasdep;
if( defined $depname ) {
	open DEP, ">>$depname" or die "Could not write $depname ($!)\n";
	$hasdep = 1;
}

print DEP "$outname:" if( $hasdep );

sub process( $ ) {
	my $file = shift;
	open FILE, $file or die "Could nod read $file ($!)\n";
	my $line;
	my $active;
	my $verbatim;
	my $buff;
	my $head;
	my $levelMark = '-';
	my $markDepth;
	while( defined( $line = <FILE> ) ) {
		chomp $line;
		if( $verbatim ) {
			if( $line =~ /\*\// ) {
				$verbatim = 0;
			} else {
				$line =~ s/^\s*\* ?//;
				print OUT "$line\n";
			}
		} elsif( $active ) {
			if( $line =~ /\*\// ) {
				$active = 0;
			} else {
				$line =~ s/^\s*\* ?//;
				$line =~ s/^\s*$/+/;
				$buff .= "$line\n";
			}
		} else {
			if( ( $line =~ /\S/ ) && ( defined $buff ) ) {
				chomp $line;
				$line =~ s/^\s*//;
				$line =~ s/\/\/.*//;
				$head .= "\n$line";
				if( $head =~ /[;{]/ ) {
					$head =~ s/\/\*.*?\*\///gs;
					$head =~ s/\s+/ /g;
					$head =~ s/([;{]).*/$1/;
					print OUT $levelMark." + +++$head+++ +\n+\n$buff\n";
					if( $head =~ /\{/ ) {
						$levelMark = '*' unless( $markDepth ++ );
					}
					$head = undef;
					$buff = undef;
				}
			} elsif( my( $head, $comment ) = ( $line =~ /^(.*)\/\*\*(.*)\*\*\// ) ) {
				$head =~ s/^\s*//;
				$head =~ s/\/\*.*?\*\///gs;
				$head =~ s/\s+/ /g;
				$head =~ s/([;{]).*/$1/;
				$comment =~ s/^\s*//;
				$comment =~ s/\s*$//;
				print OUT $levelMark." + +++$head+++ +\n+\n$comment\n\n";
				if( $head =~ /\{/ ) {
					$levelMark = '*' unless( $markDepth ++ );
				}
			} elsif( $line =~ /\}/ && $markDepth ) {
				$levelMark = '-' unless( -- $markDepth );
			} elsif( $line =~ /\/\*\*\*/ ) {
				$verbatim = 1;
			} elsif( $line =~ /\/\*\*/ ) {
				$active = 1;
			}
		}
	}
	close FILE;
}

my $line;
while( defined( $line = <IN> ) ) {
	chomp $line;
	if( my( $fname ) = ( $line =~ /^!!\s*(.*\S)/ ) ) {
		$fname = "$basedir/$fname" if( ( $fname !~ /^\// ) && defined $basedir );
		process( $fname );
		print DEP " $fname" if( $hasdep );
	} else {
		print OUT "$line\n";
	}
}

print DEP "\n" if( $hasdep );

close IN;
close OUT;
close DEP;
