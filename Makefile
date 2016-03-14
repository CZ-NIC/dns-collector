.PHONY: all clean docs dnscol build-deps build-deps-libucw build-deps-liblz4 clean-deps clean-deps-libucw clean-deps-liblz4

all: dnscol

clean:
	cd src && make clean
	rm -rf docs/html

dnscol: build-deps-libucw build-deps-liblz4
	cd src && make dnscol

docs:
	doxygen Doxyfile

build-deps: build-deps-libucw build-deps-liblz4

clean-deps: clean-deps-libucw clean-deps-liblz4

## libs/libUCW

build-deps-libucw: libs/libucw/run/lib/libucw-6.5.a
libs/libucw/run/lib/libucw-6.5.a:
	git submodule init
	git submodule update
	cd libs/libucw && \
	    ./configure -CONFIG_UCW_PERL -CONFIG_XML -CONFIG_JSON CONFIG_DEBUG CONFIG_LOCAL -CONFIG_SHARED -CONFIG_DOC -CONFIG_CHARSET && \
	    make all

clean-deps-libucw:
	cd libs/libucw && make clean

## libs/libLZ4

build-deps-liblz4: libs/lz4/lib/liblz4.a
libs/lz4/lib/liblz4.a:
	git submodule init
	git submodule update
	cd libs/lz4 && \
	    make lib

clean-deps-liblz4:
	cd libs/lz4 && \
	    make clean

