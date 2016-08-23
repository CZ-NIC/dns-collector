.PHONY: all clean docs dnscol help build-deps clean-deps

all: dnscol

help:
	@echo "all          build dnscol dependencies (build-deps) and dnscol binary"
	@echo "docs         generate Doxygen developer docs"
	@echo "clean        remove dnscol binaries and Doxygen documentation"
	@echo "build-deps   update libucw submodule and build the library"
	@echo "clean-deps   clean the libucw binaries"

clean:
	cd src && make clean
	rm -rf docs/html

dnscol: build-deps
	cd src && make dnscol

docs:
	doxygen Doxyfile

## libs/libUCW

build-deps: libs/libucw/run/lib/libucw-6.5.a

libs/libucw/run/lib/libucw-6.5.a:
	git submodule init
	git submodule update
	cd libs/libucw && \
	    ./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG CONFIG_LOCAL \
	                -CONFIG_SHARED -CONFIG_DOC -CONFIG_CHARSET && \
	    make all

clean-deps:
	cd libs/libucw && make clean


