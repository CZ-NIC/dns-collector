# Tests of string routines

Run:	../obj/ucw/str-hex-t
Out:	FEEDF00D
	fe:ed:f0:0d
	feedf00d

Run:	../obj/ucw/str-esc-t '12\r\n\000\\\xff'
Out:	31 32 0d 0a 00 5c ff

Run:	../obj/ucw/str-esc-t '\100\10a\1a'
Out:	40 08 61 01 61

Run:	../obj/ucw/str-esc-t '\a\b\f\r\n\t\v\?\"'"\\'"
Out:	07 08 0c 0d 0a 09 0b 3f 22 27

Name:	prefix1
Run:	../obj/ucw/str-fix-t p homo.sapiens.neanderthalensis homo.sap
Out:	YES

Name:	prefix2
Run:	../obj/ucw/str-fix-t p homo.sapiens.neanderthalensis homo.sapiens.neanderthalensis
Out:	YES

Name:	prefix3
Run:	../obj/ucw/str-fix-t p homo.sapiens.neanderthalensis ""
Out:	YES

Name:	prefix4
Run:	../obj/ucw/str-fix-t p homo.sapiens.neanderthalensis homo.sapiens.neanderthalensisis
Out:	NO

Name:	suffix1
Run:	../obj/ucw/str-fix-t s homo.sapiens.neanderthalensis ensis
Out:	YES

Name:	suffix2
Run:	../obj/ucw/str-fix-t s homo.sapiens.neanderthalensis homo.sapiens.neanderthalensis
Out:	YES

Name:	suffix3
Run:	../obj/ucw/str-fix-t s homo.sapiens.neanderthalensis ""
Out:	YES

Name:	suffix4
Run:	../obj/ucw/str-fix-t s homo.sapiens.neanderthalensis ecce.homo.sapiens.neanderthalensis
Out:	NO

Name:	hier-prefix1
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis homo.sap
Out:	NO

Name:	hier-prefix2
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis homo.sapiens
Out:	YES

Name:	hier-prefix3
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis homo.sapiens.
Out:	YES

Name:	hier-prefix4
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis homo.sapiens.neanderthalensis
Out:	YES

Name:	hier-prefix5
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis homo.sapiens.neanderthalensis.
Out:	NO

Name:	hier-prefix6
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis ""
Out:	YES

Name:	hier-suffix1
Run:	../obj/ucw/str-fix-t S homo.sapiens.neanderthalensis ensis
Out:	NO

Name:	hier-suffix2
Run:	../obj/ucw/str-fix-t S homo.sapiens.neanderthalensis sapiens.neanderthalensis
Out:	YES

Name:	hier-suffix3
Run:	../obj/ucw/str-fix-t S homo.sapiens.neanderthalensis .sapiens.neanderthalensis
Out:	YES

Name:	hier-suffix4
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis homo.sapiens.neanderthalensis
Out:	YES

Name:	hier-suffix5
Run:	../obj/ucw/str-fix-t P homo.sapiens.neanderthalensis .homo.sapiens.neanderthalensis
Out:	NO

Name:	hier-suffix6
Run:	../obj/ucw/str-fix-t S homo.sapiens.neanderthalensis ""
Out:	YES

Name:	hier-suffix7
Run:	../obj/ucw/str-fix-t S homo.sapiens.neanderthalensis ecce.homo.sapiens.neanderthalensis
Out:	NO
