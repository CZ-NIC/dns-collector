# Makefile for the Sherlock Library (c) 1997--2002 Martin Mares <mj@ucw.cz>

DIRS+=lib
PROGS+=obj/lib/db-tool obj/lib/buckettool

LIBSH_MODS=alloc alloc_str ctmatch db fastbuf fb-file fb-mem lists \
	log log2 md5 md5hex mmap pagecache patimatch patmatch pool \
	prime random realloc regex timer url wildmatch \
	wordsplit str_ctype str_upper bucket conf object sorter \
	finger proctitle ipaccess profile bitsig randomkey \
	hashfunc base64 base224 fb-temp printf fb-mmap
LIBSH_MOD_PATHS=$(addprefix obj/lib/,$(LIBSH_MODS)) $(CUSTOM_MODULES)

obj/lib/libsh.a: $(addsuffix .o,$(LIBSH_MOD_PATHS))
obj/lib/libsh.so: $(addsuffix .oo,$(LIBSH_MOD_PATHS))

obj/lib/hashfunc.o obj/lib/hashfunc.oo: CFLAGS += -funroll-loops

obj/lib/db-test: obj/lib/db-test.o obj/lib/libsh.$(LS)
obj/lib/db-tool: obj/lib/db-tool.o obj/lib/libsh.$(LS)
obj/lib/buckettool: obj/lib/buckettool.o obj/lib/libsh.$(LS)
obj/lib/conf-test: obj/lib/conf-test.o obj/lib/libsh.$(LS)
obj/lib/sort-test: obj/lib/sort-test.o obj/lib/libsh.$(LS)
obj/lib/lfs-test: obj/lib/lfs-test.o obj/lib/libsh.$(LS)
obj/lib/regex-test: obj/lib/regex-test.o obj/lib/libsh.$(LS)
obj/lib/hash-test: obj/lib/hash-test.o obj/lib/libsh.$(LS)
obj/lib/str-test: obj/lib/str-test.o obj/lib/libsh.$(LS)

include lib/perl/Makefile
include lib/shell/Makefile
