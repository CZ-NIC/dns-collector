# Makefile for the UCW Library (c) 1997--2004 Martin Mares <mj@ucw.cz>

DIRS+=lib

ifdef CONFIG_UCW_DBTOOL
PROGS+=obj/lib/db-tool
endif

LIBUCW_MODS= \
	alloc alloc_str realloc mempool mempool-str mempool-fmt \
	mmap pagecache partmap hashfunc \
	lists sorter bitsig \
	log log-file proctitle \
	conf ipaccess \
	profile \
	fastbuf ff-printf ff-utf8 \
	fb-file carefulio fb-mem fb-temp fb-mmap fb-limfd fb-buffer \
	str_ctype str_upper str_lower unicode-utf8 \
	wildmatch wordsplit ctmatch patimatch patmatch regex \
	prime random timer log2 randomkey \
	db \
	url \
	mainloop exitstatus runcmd sighandler \
	lizard lizard-safe adler32 \
	md5 md5hex \
	base64 base224 \
	sync

ifdef CONFIG_OWN_REGEX
include lib/regex/Makefile
endif

LIBUCW=obj/lib/libucw.$(LS)
LIBUCW_MOD_PATHS=$(addprefix obj/lib/,$(LIBUCW_MODS)) $(CUSTOM_LIB_MODULES)

obj/lib/libucw.a: $(addsuffix .o,$(LIBUCW_MOD_PATHS))
obj/lib/libucw.so: $(addsuffix .oo,$(LIBUCW_MOD_PATHS))

obj/lib/hashfunc.o obj/lib/hashfunc.oo: CFLAGS += -funroll-loops
obj/lib/lizard.o: CFLAGS += -O6 -funroll-loops

obj/lib/db-test: obj/lib/db-test.o $(LIBUCW)
obj/lib/db-tool: obj/lib/db-tool.o $(LIBUCW)
obj/lib/conf-test: obj/lib/conf-test.o $(LIBUCW)
obj/lib/sort-test: obj/lib/sort-test.o $(LIBUCW)
obj/lib/lfs-test: obj/lib/lfs-test.o $(LIBUCW)
obj/lib/hash-test: obj/lib/hash-test.o $(LIBUCW)
obj/lib/str-test: obj/lib/str-test.o $(LIBUCW)
obj/lib/asort-test: obj/lib/asort-test.o $(LIBUCW)
obj/lib/redblack-test: obj/lib/redblack-test.o $(LIBUCW)
obj/lib/binheap-test: obj/lib/binheap-test.o $(LIBUCW)
obj/lib/lizard-test: obj/lib/lizard-test.o $(LIBUCW)

TESTS+=$(addprefix obj/lib/,regex.test unicode-utf8.test hash-test.test mempool.test)
obj/lib/regex.test: obj/lib/regex-t
obj/lib/unicode-utf8.test: obj/lib/unicode-utf8-t
obj/lib/hash-test.test: obj/lib/hash-test
obj/lib/mempool.test: obj/lib/mempool-fmt-t obj/lib/mempool-str-t

ifdef CONFIG_UCW_PERL
include lib/perl/Makefile
endif

ifdef CONFIG_UCW_SHELL_UTILS
include lib/shell/Makefile
endif
