# Makefile for the UCW Library (c) 1997--2014 Martin Mares <mj@ucw.cz>

DIRS+=ucw
LIBUCW=$(o)/ucw/libucw.pc

ifdef CONFIG_UCW_UTILS
include $(s)/ucw/utils/Makefile
endif

LIBUCW_MODS= \
	threads \
	alloc alloc_str alloc-std \
	bigalloc mempool mempool-str mempool-fmt eltpool \
	partmap hashfunc \
	slists simple-lists bitsig \
	log log-stream log-file log-syslog log-conf tbf \
	conf-context conf-alloc conf-dump conf-input conf-intr conf-journal conf-parse conf-section conf-getopt \
	ipaccess \
	fastbuf ff-binary ff-string ff-printf ff-unicode ff-varint ff-stkstring \
	fb-file fb-mem fb-temp tempfile fb-mmap fb-limfd fb-buffer fb-grow fb-pool fb-atomic fb-param fb-socket fb-multi fb-null \
	char-cat char-upper char-lower unicode varint stkstring \
	wildmatch regex \
	prime primetable random \
	time-stamp time-timer time-conf \
	bit-ffs bit-fls bit-array \
	url \
	mainloop main-block main-rec \
	proctitle exitstatus runcmd \
	lizard lizard-safe adler32 sighandler \
	md5 sha1 sha1-hmac crc crc-tables \
	base64 base224 \
	io-careful io-sync io-mmap io-size \
	string str-esc str-split str-match str-imatch str-hex str-fix \
	bbuf gary \
	getopt \
	strtonum \
	resource trans res-fd res-mem res-subpool res-mempool res-eltpool \
	daemon daemon-ctrl \
	signames \
	opt opt-help opt-conf \
	table table-types xtypes xtypes-basic

LIBUCW_MAIN_INCLUDES= \
	lib.h log.h tbf.h threads.h time.h \
	alloc.h mempool.h eltpool.h \
	clists.h slists.h simple-lists.h \
	string.h stkstring.h unicode.h varint.h chartype.h regex.h \
	wildmatch.h \
	unaligned.h \
	bbuf.h gbuf.h gary.h bitarray.h bitsig.h \
	hashfunc.h hashtable.h \
	heap.h binheap.h binheap-node.h \
	redblack.h \
	prime.h \
	bitops.h \
	conf.h getopt.h ipaccess.h \
	fastbuf.h io.h ff-unicode.h ff-varint.h ff-binary.h fb-socket.h \
	url.h \
	mainloop.h \
	process.h \
	lizard.h \
	md5.h sha1.h crc.h \
	base64.h base224.h \
	kmp.h kmp-search.h trie.h binsearch.h \
	partmap.h \
	strtonum.h \
	resource.h trans.h \
	daemon.h \
	signames.h \
	sighandler.h \
	opt.h \
	table.h table-types.h \
	xtypes.h

ifdef CONFIG_UCW_THREADS
# Some modules require threading
LIBUCW_MODS+=threads-conf workqueue asio
LIBUCW_MAIN_INCLUDES+=workqueue.h semaphore.h asio.h
endif

ifdef CONFIG_UCW_FB_DIRECT
LIBUCW_MODS+=fb-direct
endif

ifdef CONFIG_UCW_OWN_GETOPT
include $(s)/ucw/getopt/Makefile
endif

LIBUCW_INCLUDES=$(LIBUCW_MAIN_INCLUDES)

include $(s)/ucw/sorter/Makefile
include $(s)/ucw/doc/Makefile

LIBUCW_MOD_PATHS=$(addprefix $(o)/ucw/,$(LIBUCW_MODS))

export LIBUCW_LIBS=-lm
ifdef CONFIG_UCW_THREADS
LIBUCW_LIBS+=-lpthread
endif
ifdef CONFIG_UCW_PCRE
LIBUCW_LIBS+=-lpcre
endif
ifdef CONFIG_UCW_MONOTONIC_CLOCK
LIBUCW_LIBS+=-lrt
endif

$(o)/ucw/libucw$(LV).a: $(addsuffix .o,$(LIBUCW_MOD_PATHS))
$(o)/ucw/libucw$(LV).so: $(addsuffix .oo,$(LIBUCW_MOD_PATHS))
$(o)/ucw/libucw$(LV).so: SONAME_SUFFIX=.0
$(o)/ucw/libucw$(LV).so: LIBS+=$(LIBUCW_LIBS)

ifdef CONFIG_INSTALL_API
$(o)/ucw/libucw.pc: $(addprefix $(o)/ucw/libucw$(LV),.a .so)
endif

$(o)/ucw/hashfunc.o $(o)/ucw/hashfunc.oo: CFLAGS += -funroll-loops
$(o)/ucw/lizard.o: CFLAGS += $(COPT2) -funroll-loops

$(o)/ucw/ff-varint-t: $(LIBUCW)
$(o)/ucw/varint-t: $(LIBUCW)
$(o)/ucw/conf-test: $(o)/ucw/conf-test.o $(LIBUCW)
$(o)/ucw/hash-test: $(o)/ucw/hash-test.o $(LIBUCW)
$(o)/ucw/hashfunc-test: $(o)/ucw/hashfunc-test.o $(LIBUCW)
$(o)/ucw/asort-test: $(o)/ucw/asort-test.o $(LIBUCW)
$(o)/ucw/redblack-test: $(o)/ucw/redblack-test.o $(LIBUCW)
$(o)/ucw/binheap-test: $(o)/ucw/binheap-test.o $(LIBUCW)
$(o)/ucw/lizard-test: $(o)/ucw/lizard-test.o $(LIBUCW)
$(o)/ucw/kmp-test: $(o)/ucw/kmp-test.o $(LIBUCW)
$(o)/ucw/strtonum-test: $(o)/ucw/strtonum-test.o $(LIBUCW)
ifdef CONFIG_CHARSET
$(o)/ucw/kmp-test: $(LIBCHARSET)
endif
$(o)/ucw/ipaccess-test: $(o)/ucw/ipaccess-test.o $(LIBUCW)
$(o)/ucw/trie-test: $(o)/ucw/trie-test.o $(LIBUCW)
$(o)/ucw/opt-test: $(o)/ucw/opt-test.o $(LIBUCW)
$(o)/ucw/table-test: $(o)/ucw/table-test.o $(LIBUCW)
$(o)/ucw/table-test-2: $(o)/ucw/table-test-2.o $(LIBUCW)
$(o)/ucw/table-test-align: $(o)/ucw/table-test-align.o $(LIBUCW)
$(o)/ucw/xtypes-test: $(o)/ucw/xtypes-test.o $(LIBUCW)

TESTS+=$(addprefix $(o)/ucw/,regex.test unicode.test hash-test.test mempool.test stkstring.test \
    slists.test bbuf.test kmp-test.test getopt.test ff-unicode.test eltpool.test \
    trie-test.test string.test sha1.test asort-test.test binheap-test.test \
    fb-file.test fb-socket.test fb-grow.test fb-pool.test fb-atomic.test fb-limfd.test fb-temp.test \
    fb-mem.test fb-buffer.test fb-mmap.test fb-multi.test fb-null.test \
    redblack-test.test url.test strtonum-test.test \
    gary.test time.test crc.test signames.test md5.test bitops.test opt.test \
    table.test table-test.test table-test-2.test table-test-align.test xtypes-test.test)

$(o)/ucw/varint.test: $(o)/ucw/varint-t
$(o)/ucw/regex.test: $(o)/ucw/regex-t
$(o)/ucw/unicode.test: $(o)/ucw/unicode-t
$(o)/ucw/hash-test.test: $(o)/ucw/hash-test
$(o)/ucw/mempool.test: $(o)/ucw/mempool-t $(o)/ucw/mempool-fmt-t $(o)/ucw/mempool-str-t
$(o)/ucw/stkstring.test: $(o)/ucw/stkstring-t
$(o)/ucw/bitops.test: $(o)/ucw/bit-ffs-t $(o)/ucw/bit-fls-t
$(o)/ucw/slists.test: $(o)/ucw/slists-t
$(o)/ucw/kmp-test.test: $(o)/ucw/kmp-test
$(o)/ucw/bbuf.test: $(o)/ucw/bbuf-t
$(o)/ucw/getopt.test: $(o)/ucw/getopt-t
$(o)/ucw/ff-unicode.test: $(o)/ucw/ff-unicode-t
$(o)/ucw/ff-varint.test: $(o)/ucw/ff-varint-t
$(o)/ucw/eltpool.test: $(o)/ucw/eltpool-t
$(o)/ucw/string.test: $(o)/ucw/str-hex-t $(o)/ucw/str-esc-t $(o)/ucw/str-fix-t
$(o)/ucw/sha1.test: $(o)/ucw/sha1-t $(o)/ucw/sha1-hmac-t
$(o)/ucw/trie-test.test: $(o)/ucw/trie-test
$(o)/ucw/asort-test.test: $(o)/ucw/asort-test
$(o)/ucw/binheap-test.test: $(o)/ucw/binheap-test
$(o)/ucw/redblack-test.test: $(o)/ucw/redblack-test
$(o)/ucw/strtonum-test.test: $(o)/ucw/strtonum-test
$(addprefix $(o)/ucw/fb-,file.test grow.test pool.test socket.test atomic.test \
	limfd.test temp.test mem.test buffer.test mmap.test multi.test null.test): %.test: %-t
$(o)/ucw/url.test: $(o)/ucw/url-t
$(o)/ucw/gary.test: $(o)/ucw/gary-t
$(o)/ucw/time.test: $(o)/ucw/time-conf-t
$(o)/ucw/crc.test: $(o)/ucw/crc-t
$(o)/ucw/signames.test: $(o)/ucw/signames-t
$(o)/ucw/md5.test: $(o)/ucw/md5-t
$(o)/ucw/opt.test: $(o)/ucw/opt-test
$(o)/ucw/table.test: $(o)/ucw/table-t
$(o)/ucw/table-test.test: $(o)/ucw/table-test
$(o)/ucw/table-test-2.test: $(o)/ucw/table-test-2
$(o)/ucw/table-test-align.test: $(o)/ucw/table-test-align
$(o)/ucw/xtypes-test.test: $(o)/ucw/xtypes-test

ifdef CONFIG_UCW_THREADS
TESTS+=$(addprefix $(o)/ucw/,asio.test)
$(o)/ucw/asio.test: $(o)/ucw/asio-t
endif

# The version of autoconf.h that is a part of the public API needs to have
# the internal symbols filtered out, so we generate ucw/autoconf.h in the
# configure script and let the public config.h refer to <ucw/autoconf.h>
# instead of plain "autoconf.h".

API_LIBS+=libucw
API_INCLUDES+=$(o)/ucw/.include-stamp
$(o)/ucw/.include-stamp: $(addprefix $(s)/ucw/,$(LIBUCW_INCLUDES)) $(o)/ucw/autoconf.h
	$(Q)$(BUILDSYS)/install-includes $(<D) run/include/ucw $(LIBUCW_INCLUDES)
	$(Q)$(BUILDSYS)/install-includes $(o)/ucw run/include/ucw autoconf.h
	$(Q)sed -e 's/^#include "autoconf\.h"/#include <ucw\/autoconf.h>/' <$(s)/ucw/config.h >run/include/ucw/config.h
	$(Q)touch $@
run/lib/pkgconfig/libucw.pc: $(o)/ucw/libucw.pc

ifdef CONFIG_UCW_PERL
include $(s)/ucw/perl/Makefile
endif

ifdef CONFIG_UCW_SHELL_UTILS
include $(s)/ucw/shell/Makefile
endif

CONFIGS+=libucw

INSTALL_TARGETS+=install-libucw-lib
install-libucw-lib:
	install -d -m 755 $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw$(LV).so.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw$(LV).so.0.0
	ln -sf libucw$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw$(LV).so.0
.PHONY: install-libucw-lib

INSTALL_TARGETS+=install-libucw-api
install-libucw-api: install-ucw-sorter-api
	install -d -m 755 $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/ucw/ $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/ucw/,$(LIBUCW_MAIN_INCLUDES) autoconf.h config.h) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/ucw/
	install -m 644 run/lib/pkgconfig/libucw.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	ln -sf libucw$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw$(LV).so
	install -m 644 run/lib/libucw$(LV).a $(DESTDIR)$(INSTALL_LIB_DIR)
.PHONY: install-libucw-api

INSTALL_TARGETS+=install-libucw-config
install-libucw-config:
	install -d -m 755 $(DESTDIR)$(INSTALL_CONFIG_DIR)
	install -m 644 run/$(CONFIG_DIR)/libucw $(DESTDIR)$(INSTALL_CONFIG_DIR)
.PHONY: install-libucw-config
