# Makefile for the Sherlock UniCode Library (c) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>

CLIB=../lib/libunicode.a

OBJS=toupper.o tolower.o tocat.o utf8.o unaccent.o tosig.o strlen.o debug.o \
	charconv.o setnames.o

all: .stamp $(CLIB)

.stamp:
	mkdir -p $(TOPDIR)/run/char
	ln -sf `pwd`/set $(TOPDIR)/run/char
	touch .stamp

$(CLIB): $(OBJS)
	rm -f $(CLIB)
	ar rcs $(CLIB) $(OBJS)

toupper.o: toupper.c unicode.h U-upper.h
tolower.o: tolower.c unicode.h U-lower.h
tocat.o: tocat.c unicode.h U-cat.h
utf8.o: utf8.c unicode.h
unaccent.o: unaccent.c unicode.h U-unacc.h
tosig.o: tosig.c unicode.h U-sig.h
strlen.o: strlen.c unicode.h
debug.o: debug.c unicode.h
charconv.o: charconv.c charconv.h chartable.h
setnames.o: setnames.c charconv.h

test: test.o $(CLIB) $(TOPDIR)/lib/libsh.a
	$(CC) $(LDFLAGS) test.o -o test -lsh -lunicode

test.o: test.c unicode.h

buildall: cleanall
	mkdir exp
	for a in `find set -type f -maxdepth 1` ; do b=`basename $$a` ; echo $$b ; misc/mkexpands $$b >exp/$$b ; done
	misc/unisplit
	misc/gentab _U_cat byte <misc/u-cat >U-cat.h
	misc/gentab _U_lower word <misc/u-lower >U-lower.h
	misc/gentab _U_upper word <misc/u-upper >U-upper.h
	misc/mkunacc >misc/u-unacc
	misc/gentab _U_unaccent word <misc/u-unacc >U-unacc.h
	misc/mksig >misc/u-sig
	misc/gentab _U_sig byte <misc/u-sig >U-sig.h
	misc/tabgen <misc/chartable.in >chartable.h

cleanall:
	rm -rf exp misc/u-* U-*.h chartable.h

tags:
	etags *.[ch]

clean:
	rm -f .stamp misc/u-* test
