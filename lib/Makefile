# Makefile for the Sherlock Library (c) 1997--2004 Martin Mares <mj@ucw.cz>

DIRS+=lib
PROGS+=obj/lib/db-tool obj/lib/buckettool
TESTS+=obj/lib/regex-ut

LIBSH_MODS= \
	alloc alloc_str realloc mempool mempool-str \
	mmap pagecache partmap hashfunc \
	lists sorter bitsig \
	log log-file proctitle \
	conf ipaccess \
	profile \
	fastbuf ff-printf ff-utf8 \
	fb-file carefulio fb-mem fb-temp fb-mmap fb-limfd fb-buffer \
	str_ctype str_upper str_lower unicode-utf8 \
	wildmatch wordsplit ctmatch patimatch patmatch regex \
	bucket object buck2obj obj2buck \
	prime random timer log2 randomkey \
	db \
	url urlkey finger \
	mainloop exitstatus runcmd sighandler \
	lizard lizard-safe lizard-fb lizard-fb adler32 \
	md5 md5hex \
	base64 base224 \
	sync

ifdef CONFIG_OWN_REGEX
include lib/regex/Makefile
endif

LIBSH_MOD_PATHS=$(addprefix obj/lib/,$(LIBSH_MODS)) $(CUSTOM_LIB_MODULES)

obj/lib/libsh.a: $(addsuffix .o,$(LIBSH_MOD_PATHS))
obj/lib/libsh.so: $(addsuffix .oo,$(LIBSH_MOD_PATHS))

obj/lib/hashfunc.o obj/lib/hashfunc.oo: CFLAGS += -funroll-loops

obj/lib/db-test: obj/lib/db-test.o $(LIBSH)
obj/lib/db-tool: obj/lib/db-tool.o $(LIBSH)
obj/lib/buckettool: obj/lib/buckettool.o $(LIBSH)
obj/lib/conf-test: obj/lib/conf-test.o $(LIBSH)
obj/lib/sort-test: obj/lib/sort-test.o $(LIBSH)
obj/lib/lfs-test: obj/lib/lfs-test.o $(LIBSH)
obj/lib/hash-test: obj/lib/hash-test.o $(LIBSH)
obj/lib/str-test: obj/lib/str-test.o $(LIBSH)
obj/lib/asort-test: obj/lib/asort-test.o $(LIBSH)
obj/lib/redblack-test: obj/lib/redblack-test.o $(LIBSH)
obj/lib/binheap-test: obj/lib/binheap-test.o $(LIBSH)
obj/lib/lizard-test: obj/lib/lizard-test.o $(LIBSH)

obj/lib/lizard.o: CFLAGS += -O6 -funroll-loops

include lib/perl/Makefile
include lib/shell/Makefile
