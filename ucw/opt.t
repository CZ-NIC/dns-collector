# Tests of the command line option parser

Name:	Opt-1
Run:	( ../obj/ucw/opt-t 2>&1 1>/dev/null || [ $? -eq "2" ] ) | tr -d '\n'
Out:	Required option -t not found.Run with argument --help for more information.

Name:	Opt-2
Run:	../obj/ucw/opt-t -t95C -w640 -gG darjeeling
Out:	English style: no|Chosen teapot: glass|Temperature: 95C|Verbosity: 1|Prayer: no|Water amount: 640|Gas: yes|First tea: darjeeling|Everything OK. Bye.

Name:	Opt-3
Run:	../obj/ucw/opt-t -vvqvqvhpe -t120F -w4 darjeeling
Out:	English style: yes|Chosen teapot: hands|Temperature: 120F|Verbosity: 3|Prayer: yes|Water amount: 4|Gas: no|First tea: darjeeling|Everything OK. Bye.

Name:	Opt-4
Run:	../obj/ucw/opt-t -t120F -w4 puerh darjeeling earl-gray
Out:	English style: no|Temperature: 120F|Verbosity: 1|Prayer: no|Water amount: 4|Gas: no|First tea: puerh|Boiling a tea: darjeeling|Boiling a tea: earl-gray|Everything OK. Bye.

Name:	Opt-5
Run:	( ../obj/ucw/opt-t -ghx 2>&1 1>/dev/null || [ $? -eq "2" ] ) | tr -d '\n'
Out:	Multiple switches: -hRun with argument --help for more information.

Name:	Opt-6
Run:	../obj/ucw/opt-t -t120F -w4 -b15 -he -- --puerh darjeeling earl-gray
Out:	English style: yes|Chosen teapot: hands|Temperature: 120F|Verbosity: 1|Black magic: 15|Prayer: no|Water amount: 4|Gas: no|First tea: --puerh|Boiling a tea: darjeeling|Boiling a tea: earl-gray|Everything OK. Bye.

Name:	Opt-7
Run:	../obj/ucw/opt-t -t120F -w4 -b15 -b54 -he -- --puerh darjeeling earl-gray
Out:	English style: yes|Chosen teapot: hands|Temperature: 120F|Verbosity: 1|Black magic: 15|Black magic: 54|Prayer: no|Water amount: 4|Gas: no|First tea: --puerh|Boiling a tea: darjeeling|Boiling a tea: earl-gray|Everything OK. Bye.

Name:	Opt-Conf-1
Run:	( ../obj/ucw/opt-t -h --dumpconfig 2>&1 1>/dev/null || [ $? -eq "2" ] ) | tr -d '\n'
Out:	Config options (-C, -S) must stand before other options.Run with argument --help for more information.

Name:	Opt-Hook-1
Run:	../obj/ucw/opt-t -Ht 95C -w640 -gG darjeeling
Out:	[HOOK-postval:H/show-hooks=(null)] [HOOK-preval:t/temperature=95C] [HOOK-postval:t/temperature=95C] [HOOK-prearg] [HOOK-preval:w/water=640] [HOOK-postval:w/water=640] [HOOK-prearg] [HOOK-preval:g/glass-set=(null)] [HOOK-postval:g/glass-set=(null)] [HOOK-preval:G/with-gas=(null)] [HOOK-postval:G/with-gas=(null)] [HOOK-prearg] [HOOK-preval:/(null)=darjeeling] [HOOK-postval:/(null)=darjeeling] English style: no|Chosen teapot: glass|Temperature: 95C|Verbosity: 1|Prayer: no|Water amount: 640|Gas: yes|First tea: darjeeling|Everything OK. Bye.
