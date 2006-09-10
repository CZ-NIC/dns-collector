# Testing dir... code will be moved somewhere else... maybe to trash :-)

DIRS+=images

PROGS+=$(o)/images/image-tool
CONFIGS+=images
LIBIMAGES_MODS=math config image scale color alpha io-main object

ifdef CONFIG_IMAGES_DUP
PROGS+=$(o)/images/image-dup-test
LIBIMAGES_MODS+=dup-init dup-cmp
endif
ifdef CONFIG_IMAGES_SIM
PROGS+=$(o)/images/image-sim-test
LIBIMAGES_MODS+=sig-dump sig-init sig-seg sig-txt sig-cmp
endif

LIBIMAGES_LIBS=-lm -lpthread

ifdef CONFIG_IMAGES_LIBJPEG
LIBIMAGES_MODS+=io-libjpeg
LIBIMAGES_LIBS+=-ljpeg
endif

ifdef CONFIG_IMAGES_LIBPNG
LIBIMAGES_MODS+=io-libpng
LIBIMAGES_LIBS+=-lpng
endif

ifdef CONFIG_IMAGES_LIBUNGIF
LIBIMAGES_MODS+=io-libungif
LIBIMAGES_LIBS+=-lungif
else
ifdef CONFIG_IMAGES_LIBGIF
LIBIMAGES_MODS+=io-libungif
LIBIMAGES_LIBS+=-lgif
endif
endif

ifdef CONFIG_IMAGES_LIBMAGICK
LIBIMAGES_MODS+=io-libmagick
MAGICK_LIBS:=$(shell GraphicsMagick-config --libs)
MAGICK_CPPFLAGS:=$(shell GraphicsMagick-config --cppflags)
LIBIMAGES_LIBS+=$(MAGICK_LIBS)
$(o)/images/io-libmagick.o: CFLAGS+=$(MAGICK_CPPFLAGS)
endif

$(o)/images/libimages.a: $(addsuffix .o,$(addprefix $(o)/images/,$(LIBIMAGES_MODS)))
$(o)/images/libimages.so: $(addsuffix .oo,$(addprefix $(o)/images/,$(LIBIMAGES_MODS)))

$(o)/images/image-tool: $(o)/images/image-tool.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-tool: LIBS+=$(LIBIMAGES_LIBS)

$(o)/images/image-dup-test: $(o)/images/image-dup-test.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-dup-test: LIBS+=$(LIBIMAGES_LIBS)

$(o)/images/image-sim-test: $(o)/images/image-sim-test.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-sim-test: LIBS+=$(LIBIMAGES_LIBS)

TESTS+=$(o)/images/image-test.test
$(o)/images/image-test: $(o)/images/image-test.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-test: LIBS+=$(LIBIMAGES_LIBS)
$(o)/images/image-test.test: $(o)/images/image-test

TESTS+=$(o)/images/hilbert-test.test
$(o)/images/hilbert-test: LIBS+=-lm $(LIBSH)
$(o)/images/hilbert-test.test: $(o)/images/hilbert-test

TESTS+=$(o)/images/color.test
$(o)/images/color-t: LIBS+=-lm
$(o)/images/color.test: $(o)/images/color-t

#$(o)/images/image-idx: $(o)/images/image-idx.o $(o)/images/image-obj.o $(o)/images/dup-cmp.o $(o)/indexer/iconfig.o $(o)/images/image-sig.o $(o)/images/kd-tree.o $(o)/images/color.o $(o)/images/image-io.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
#$(o)/images/image-idx: LIBS+=-lGraphicsMagick -ljpeg -lpng

# By :;DF
#PROGS+=$(addprefix $(o)/images/,decomp)
#
#$(o)/images/block_info.o $(o)/images/block_info.oo: CFLAGS+=-I/usr/include/GraphicsMagick
#$(o)/images/k_means.o $(o)/images/k_means.oo: CFLAGS+=-I/usr/include/GraphicsMagick
#$(o)/images/decomp.o $(o)/images/decomp.oo: CFLAGS+=-I/usr/include/GraphicsMagick

#$(o)/images/decomp: $(o)/images/decomp.o $(o)/images/block_info.o $(o)/images/k_means.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
#$(o)/images/decomp: LIBS+=-lGraphicsMagick -ljpeg -lm

