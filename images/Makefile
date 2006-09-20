# Testing dir... code will be moved somewhere else... maybe to trash :-)

DIRS+=images

PROGS+=$(o)/images/image-tool $(o)/images/color-tool
CONFIGS+=images
LIBIMAGES_MODS=math config context image scale color alpha io-main object

ifdef CONFIG_IMAGES_DUP
PROGS+=$(o)/images/image-dup-test
LIBIMAGES_MODS+=dup-init dup-cmp
endif
ifdef CONFIG_IMAGES_SIM
PROGS+=$(o)/images/image-sim-test
LIBIMAGES_MODS+=sig-cmp
endif
ifneq ($(CONFIG_IMAGES_DUP)$(CONFIG_IMAGES_SIM),)
LIBIMAGES_MODS+=sig-dump sig-init sig-seg sig-txt
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

$(o)/images/color-tool: $(o)/images/color-tool.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/color-tool: LIBS+=$(LIBIMAGES_LIBS)

$(o)/images/image-dup-test: $(o)/images/image-dup-test.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-dup-test: LIBS+=$(LIBIMAGES_LIBS)

$(o)/images/image-sim-test: $(o)/images/image-sim-test.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-sim-test: LIBS+=$(LIBIMAGES_LIBS)

TESTS+=$(o)/images/image-test.test
$(o)/images/image-test: $(o)/images/image-test.o $(LIBIMAGES) $(LIBUCW)
$(o)/images/image-test: LIBS+=$(LIBIMAGES_LIBS) -lpthread
$(o)/images/image-test.test: $(o)/images/image-test

TESTS+=$(o)/images/hilbert-test.test
$(o)/images/hilbert-test: LIBS+=-lm $(LIBSH)
$(o)/images/hilbert-test.test: $(o)/images/hilbert-test

TESTS+=$(o)/images/color.test
$(o)/images/color-t: LIBS+=-lm
$(o)/images/color.test: $(o)/images/color-t
