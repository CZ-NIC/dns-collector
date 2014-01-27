# Tests of the command line option parser

Name:	Opt-1
Run:	../obj/ucw/opt-test 2>&1 1>/dev/null
Exit:	2
Out:	Required option -t/--temperature not found.
	Run with argument --help for more information.

Name:	Opt-2
Run:	../obj/ucw/opt-test -t95C -w640 -gG darjeeling
Out:	English style: no|Chosen teapot: glass|Temperature: 95C|Verbosity: 1|Prayer: no|Water amount: 640|Gas: yes|First tea: darjeeling|Everything OK. Bye.

Name:	Opt-3
Run:	../obj/ucw/opt-test -vvqvqvhpe -t120F -w4 darjeeling
Out:	English style: yes|Chosen teapot: hands|Temperature: 120F|Verbosity: 3|Prayer: yes|Water amount: 4|Gas: no|First tea: darjeeling|Everything OK. Bye.

Name:	Opt-4
Run:	../obj/ucw/opt-test -t120F -w4 puerh darjeeling earl-grey
Out:	English style: no|Temperature: 120F|Verbosity: 1|Prayer: no|Water amount: 4|Gas: no|First tea: puerh|Boiling a tea: darjeeling|Boiling a tea: earl-grey|Everything OK. Bye.

Name:	Opt-5
Run:	../obj/ucw/opt-test -ghx 2>&1 1>/dev/null || [ $? -eq "2" ]
Out:	Multiple switches: -h
	Run with argument --help for more information.

Name:	Opt-6
Run:	../obj/ucw/opt-test -t120F -w4 -b15 -he -- --puerh darjeeling earl-grey
Out:	English style: yes|Chosen teapot: hands|Temperature: 120F|Verbosity: 1|Black magic: 15|Prayer: no|Water amount: 4|Gas: no|First tea: --puerh|Boiling a tea: darjeeling|Boiling a tea: earl-grey|Everything OK. Bye.

Name:	Opt-7
Run:	../obj/ucw/opt-test -t120F -w4 -b15 -b54 -he -- --puerh darjeeling earl-grey
Out:	English style: yes|Chosen teapot: hands|Temperature: 120F|Verbosity: 1|Black magic: 15|Black magic: 54|Prayer: no|Water amount: 4|Gas: no|First tea: --puerh|Boiling a tea: darjeeling|Boiling a tea: earl-grey|Everything OK. Bye.

Name:	Opt-Conf-1
Run:	../obj/ucw/opt-test -h --dumpconfig 2>&1 1>/dev/null || [ $? -eq "2" ]
Out:	Config options (-C, -S) must stand before other options.
	Run with argument --help for more information.

Name:	Opt-Hook-1
Run:	../obj/ucw/opt-test -Ht 95C -w640 -gG darjeeling
Out:	[HOOK-postval:H/show-hooks=(null)] [HOOK-preval:t/temperature=95C] [HOOK-postval:t/temperature=95C] [HOOK-prearg] [HOOK-preval:w/water=640] [HOOK-postval:w/water=640] [HOOK-prearg] [HOOK-preval:g/glass-set=(null)] [HOOK-postval:g/glass-set=(null)] [HOOK-preval:G/with-gas=(null)] [HOOK-postval:G/with-gas=(null)] [HOOK-prearg] [HOOK-preval:Å/(null)=darjeeling] [HOOK-postval:Å/(null)=darjeeling] English style: no|Chosen teapot: glass|Temperature: 95C|Verbosity: 1|Prayer: no|Water amount: 640|Gas: yes|First tea: darjeeling|Everything OK. Bye.
