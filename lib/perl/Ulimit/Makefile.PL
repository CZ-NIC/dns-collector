# Makefile for Perl MakeMaker  (c) 2003 Tomas Valla <tom@ucw.cz>

use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'		=> 'Sherlock::Ulimit',
    'MAKEFILE'		=> 'Makefile.tmp',
    'VERSION_FROM'	=> 'Ulimit.pm',
    'INST_LIB'		=> 'lib',
    'INST_ARCHLIB'	=> 'arch',
);
