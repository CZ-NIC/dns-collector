# Makefile for the Sherlock Library (c) 1997--2002 Martin Mares <mj@ucw.cz>

DIRS+=lib
PROGS+=obj/lib/db-tool obj/lib/buckettool

SHLIB_OBJS=alloc.o alloc_str.o ctmatch.o db.o fastbuf.o fb-file.o fb-mem.o lists.o \
	log.o log2.o md5.o md5hex.o mmap.o pagecache.o patimatch.o patmatch.o pool.o \
	prime.o random.o realloc.o regex.o timer.o url.o wildmatch.o \
	wordsplit.o str_ctype.o str_upper.o bucket.o conf.o object.o sorter.o \
	finger.o proctitle.o ipaccess.o profile.o bitsig.o randomkey.o \
	hashfunc.o base224.o fb-temp.o

obj/lib/libsh.a: $(addprefix obj/lib/,$(SHLIB_OBJS)) $(CUSTOM_MODULES)

obj/lib/hashfunc.o: CFLAGS += -funroll-loops

obj/lib/db-test: obj/lib/db-test.o obj/lib/libsh.a
obj/lib/db-tool: obj/lib/db-tool.o obj/lib/libsh.a
obj/lib/buckettool: obj/lib/buckettool.o obj/lib/libsh.a
obj/lib/conf-test: obj/lib/conf-test.o obj/lib/libsh.a
obj/lib/sort-test: obj/lib/sort-test.o obj/lib/libsh.a
obj/lib/lfs-test: obj/lib/lfs-test.o obj/lib/libsh.a
obj/lib/regex-test: obj/lib/regex-test.o obj/lib/libsh.a
obj/lib/hash-test: obj/lib/hash-test.o obj/lib/libsh.a
obj/lib/str-test: obj/lib/str-test.o obj/lib/libsh.a
