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

sub formatNote( $$ ) {
        my( $head, $comment ) = @_;
	$head =~ s/[\t ]+/ /g;
	print OUT "\n";
        print OUT "''''\n";
	print OUT "..................\n";
        print OUT "$head\n";
        print OUT "..................\n\n";
        print OUT "$comment\n";
}

sub process( $ ) {
	my $file = shift;
	open FILE, $file or die "Could nod read $file ($!)\n";
	my $line;
	my $active;
	my $verbatim;
	my $buff;
	my $head;
	my $struct;
	while( defined( $line = <FILE> ) ) {
		chomp $line;
		if( $struct ) {
			$head .= "\n".$line;
			if( $line =~ /}/ ) {
				formatNote( $head, $buff );
				$struct = 0;
				$buff = undef;
				$head = undef;
			}
		} elsif( $verbatim ) {
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
				if( $line =~ /\(/ || $line !~ /{/ ) {
					$_ = $line;
					s/^\s*\s?//;
					s/\/\/.*//;
				        s/\/\*.*?\*\///gs;
					s/[;{].*//;
					formatNote( $_, $buff );
					$head = undef;
					$buff = undef;
				} else {
					$head = $line;
					$struct = 1;
				}
			} elsif( ( $head, $buff ) = ( $line =~ /^(.*)\/\*\*(.*)\*\*\// ) ) {
				$buff =~ s/^\s*//;
				$buff =~ s/\s*$//;
				if( $head =~ /\(/ || $head !~ /{/ ) {
					$head =~ s/^\s*//;
					$head =~ s/\/\*.*?\*\///gs;
					$head =~ s/([;{]).*/$1/;
					formatNote( $head, $buff );
					$head = undef;
					$buff = undef;
				} else {
					$struct = 1;
				}
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
