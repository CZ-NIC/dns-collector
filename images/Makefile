# Testing dir... code will be moved somewhere else... maybe to trash :-)

DIRS+=images

PROGS+=$(addprefix $(o)/images/,image-tool)

LIBIMAGES_MODS=image scale io-main
LIBIMAGES=$(o)/images/libimages.$(LS)
LIBIMAGES_LIBS=

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

TESTS+=$(o)/images/hilbert-test.test
$(o)/images/hilbert-test: LIBS+=-lm $(LIBSH)
$(o)/images/hilbert-test.test: $(o)/images/hilbert-test

#$(o)/images/image-test: $(o)/images/image-test.o $(LIBIMAGES) $(LIBUCW)
#$(o)/images/image-test: LIBS+=$(LIBIMAGES_LIBS)

#$(o)/images/image-idx: $(o)/images/image-idx.o $(o)/images/image-obj.o $(o)/images/dup-cmp.o $(o)/indexer/iconfig.o $(o)/images/image-sig.o $(o)/images/kd-tree.o $(o)/images/color.o $(o)/images/image-io.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
#$(o)/images/image-idx: LIBS+=-lGraphicsMagick -ljpeg -lpng

#$(o)/images/color-t: LIBS+=-lm
#$(o)/images/color.test: $(o)/images/color-t

# By :;DF
#PROGS+=$(addprefix $(o)/images/,decomp)
#
#$(o)/images/block_info.o $(o)/images/block_info.oo: CFLAGS+=-I/usr/include/GraphicsMagick
#$(o)/images/k_means.o $(o)/images/k_means.oo: CFLAGS+=-I/usr/include/GraphicsMagick
#$(o)/images/decomp.o $(o)/images/decomp.oo: CFLAGS+=-I/usr/include/GraphicsMagick

#$(o)/images/decomp: $(o)/images/decomp.o $(o)/images/block_info.o $(o)/images/k_means.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
#$(o)/images/decomp: LIBS+=-lGraphicsMagick -ljpeg -lm

