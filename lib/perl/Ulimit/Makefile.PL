use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'Sherlock::Ulimit',
    'MAKEFILE'		=> 'Makefile.tmp',
    'VERSION_FROM'	=> 'Ulimit.pm',
    'INST_LIB'		=> 'lib',
    'INST_ARCHLIB'	=> 'arch',
    #'PREFIX'  => '.',
);
