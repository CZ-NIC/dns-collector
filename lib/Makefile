# Makefile for the UCW Library (c) 1997--2006 Martin Mares <mj@ucw.cz>

DIRS+=lib

ifdef CONFIG_UCW_DBTOOL
PROGS+=$(o)/lib/db-tool
endif

LIBUCW_MODS= \
	alloc alloc_str realloc mempool mempool-str mempool-fmt \
	mmap pagecache partmap hashfunc \
	lists slists simple-lists sorter bitsig \
	log log-file proctitle \
	conf-alloc conf-dump conf-input conf-intr conf-journal conf-parse conf-section \
	ipaccess \
	profile \
	fastbuf ff-binary ff-string ff-printf ff-utf8 \
	fb-file carefulio fb-mem fb-temp fb-mmap fb-limfd fb-buffer fb-grow \
	str_ctype str_upper str_lower unicode-utf8 stkstring \
	wildmatch wordsplit ctmatch patimatch patmatch regex \
	prime primetable random timer randomkey \
	bit-ffs bit-fls \
	db \
	url \
	mainloop exitstatus runcmd sighandler \
	lizard lizard-safe adler32 \
	md5 md5hex \
	base64 base224 \
	sync \
	qache \
	string \
	bbuf

LIBUCW_INCLUDES= \
	lib.h config.h math.h \
	mempool.h pagecache.h \
	sorter.h arraysort.h \
	lists.h clists.h \
	unaligned.h prefetch.h \
	bbuf.h gbuf.h bitarray.h bitsig.h \
	hashfunc.h hashtable.h \
	heap.h binheap.h binheap-node.h \
	redblack.h \
	conf.h getopt.h ipaccess.h \
	profile.h \
	fastbuf.h lfs.h ff-utf8.h \
	chartype.h unicode.h stkstring.h \
	wildmatch.h patmatch.h \
	db.h \
	url.h \
	mainloop.h \
	lizard.h \
	md5.h \
	base64.h base224.h \
	qache.h

ifdef CONFIG_OWN_REGEX
include $(s)/lib/regex/Makefile
endif

LIBUCW=$(o)/lib/libucw.$(LS)
LIBUCW_MOD_PATHS=$(addprefix $(o)/lib/,$(LIBUCW_MODS))

$(o)/lib/libucw.a: $(addsuffix .o,$(LIBUCW_MOD_PATHS))
$(o)/lib/libucw.so: $(addsuffix .oo,$(LIBUCW_MOD_PATHS))

$(o)/lib/hashfunc.o $(o)/lib/hashfunc.oo: CFLAGS += -funroll-loops
$(o)/lib/lizard.o: CFLAGS += $(COPT2) -funroll-loops

$(o)/lib/db-test: $(o)/lib/db-test.o $(LIBUCW)
$(o)/lib/db-tool: $(o)/lib/db-tool.o $(LIBUCW)
$(o)/lib/conf-test: $(o)/lib/conf-test.o $(LIBUCW)
$(o)/lib/sort-test: $(o)/lib/sort-test.o $(LIBUCW)
$(o)/lib/lfs-test: $(o)/lib/lfs-test.o $(LIBUCW)
$(o)/lib/hash-test: $(o)/lib/hash-test.o $(LIBUCW)
$(o)/lib/str-test: $(o)/lib/str-test.o $(LIBUCW)
$(o)/lib/asort-test: $(o)/lib/asort-test.o $(LIBUCW)
$(o)/lib/redblack-test: $(o)/lib/redblack-test.o $(LIBUCW)
$(o)/lib/binheap-test: $(o)/lib/binheap-test.o $(LIBUCW)
$(o)/lib/lizard-test: $(o)/lib/lizard-test.o $(LIBUCW)
$(o)/lib/kmp-test: $(o)/lib/kmp-test.o $(LIBUCW) $(LIBCHARSET)
$(o)/lib/ipaccess-test: $(o)/lib/ipaccess-test.o $(LIBUCW)

TESTS+=$(addprefix $(o)/lib/,regex.test unicode-utf8.test hash-test.test mempool.test stkstring.test slists.test kmp-test.test bbuf.test)
$(o)/lib/regex.test: $(o)/lib/regex-t
$(o)/lib/unicode-utf8.test: $(o)/lib/unicode-utf8-t
$(o)/lib/hash-test.test: $(o)/lib/hash-test
$(o)/lib/mempool.test: $(o)/lib/mempool-fmt-t $(o)/lib/mempool-str-t
$(o)/lib/stkstring.test: $(o)/lib/stkstring-t
$(o)/lib/bitops.test: $(o)/lib/bit-ffs-t $(o)/lib/bit-fls-t
$(o)/lib/slists.test: $(o)/lib/slists-t
$(o)/lib/kmp-test.test: $(o)/lib/kmp-test
$(o)/lib/bbuf.test: $(o)/lib/bbuf-t

INCLUDES+=$(o)/lib/.include-stamp
$(o)/lib/.include-stamp: $(addprefix $(s)/lib/,$(LIBUCW_INCLUDES))
	$(s)/build/install-includes $(s)/lib run/include/lib $(?F)
	$(s)/build/install-includes $(o)/lib run/include/lib autoconf.h
	touch $(o)/lib/.include-stamp

ifdef CONFIG_UCW_PERL
include $(s)/lib/perl/Makefile
endif

ifdef CONFIG_UCW_SHELL_UTILS
include $(s)/lib/shell/Makefile
endif
