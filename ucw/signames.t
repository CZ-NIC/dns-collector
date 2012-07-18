# Tests for signames.c

Name:	Name -> number
Run:	../obj/ucw/signames-t
In:	SIGKILL
	SIGSEGV
	sigterm
Out:	9
	11
	?

Name:	Number -> name
Run:	../obj/ucw/signames-t
In:	#9
	#11
	#0
Out:	SIGKILL
	SIGSEGV
	?

