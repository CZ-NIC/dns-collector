# Makefile for the Sherlock Charset Data Files (c) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>

all: .stamp

.stamp:
	mkdir -p $(TOPDIR)/run/char
	ln -sf `pwd`/set $(TOPDIR)/run/char
	ln -sf `pwd`/exp $(TOPDIR)/run/char
	touch .stamp

buildall:
	rm -rf exp
	mkdir exp
	for a in `find set -type f` ; do b=`basename $$a` ; echo $$b ; misc/mkexpands $$b >exp/$$b ; done

tags:

clean:
	rm -f .stamp
