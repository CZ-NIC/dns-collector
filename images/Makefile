# Makefile for the Image Library (c) 2006 Pavel Charvat <pchar@ucw.cz>

DIRS+=images

LIBIMAGES_PROGS=$(o)/images/image-tool $(o)/images/color-tool
LIBIMAGES_CONFIGS+=images
LIBIMAGES_MODS=math config context image scale color io-main
LIBIMAGES_INCLUDES=images.h error.h color.h math.h
export LIBIMAGES_LIBS=

ifdef CONFIG_SHERLOCK
LIBIMAGES_MODS+=object
LIBIMAGES_DEPS=$(LIBSH)
else
LIBIMAGES_DEPS=$(LIBUCW)
endif

ifdef CONFIG_STATIC_PIC
LIBIMAGES_DEPS+=$(o)/images/libucw-images-pic.a
endif

ifdef CONFIG_IMAGES_DUP
LIBIMAGES_PROGS+=$(o)/images/image-dup-test
LIBIMAGES_MODS+=dup-init dup-cmp
LIBIMAGES_INCLUDES+=duplicates.h
endif
ifdef CONFIG_IMAGES_SIM
LIBIMAGES_PROGS+=$(o)/images/image-sim-test
LIBIMAGES_MODS+=sig-cmp
endif
ifneq ($(CONFIG_IMAGES_DUP)$(CONFIG_IMAGES_SIM),)
LIBIMAGES_MODS+=sig-dump sig-init sig-seg sig-txt
LIBIMAGES_INCLUDES+=signature.h
endif

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

PROGS+=$(LIBIMAGES_PROGS)
CONFIGS+=$(LIBIMAGES_CONFIGS)

$(o)/images/libucw-images.a: $(addsuffix .o,$(addprefix $(o)/images/,$(LIBIMAGES_MODS)))
$(o)/images/libucw-images-pic.a: $(addsuffix .oo,$(addprefix $(o)/images/,$(LIBIMAGES_MODS)))
$(o)/images/libucw-images.so: $(addsuffix .oo,$(addprefix $(o)/images/,$(LIBIMAGES_MODS)))
$(o)/images/libucw-images.pc: $(LIBIMAGES_DEPS)

$(o)/images/image-tool: $(o)/images/image-tool.o $(LIBIMAGES)
$(o)/images/color-tool: $(o)/images/color-tool.o $(LIBIMAGES)
$(o)/images/image-dup-test: $(o)/images/image-dup-test.o $(LIBIMAGES)
$(o)/images/image-sim-test: $(o)/images/image-sim-test.o $(LIBIMAGES)

TESTS+=$(o)/images/image-test.test
$(o)/images/image-test: $(o)/images/image-test.o $(LIBIMAGES)
$(o)/images/image-test.test: $(o)/images/image-test

TESTS+=$(o)/images/color.test
$(o)/images/color-t: $(LIBIMAGES)
$(o)/images/color.test: $(o)/images/color-t

API_LIBS+=libucw-images
API_INCLUDES+=$(o)/images/.include-stamp
$(o)/images/.include-stamp: $(addprefix $(s)/images/,$(LIBIMAGES_INCLUDES))
$(o)/images/.include-stamp: IDST=images
run/lib/pkgconfig/libucw-images.pc: $(o)/images/libucw-images.pc

INSTALL_TARGETS+=install-libucw-images
install-libucw-images:
	install -d -m 755 $(addprefix $(DESTDIR),$(INSTALL_BIN_DIR) $(INSTALL_INCLUDE_DIR)/images $(INSTALL_LIB_DIR) $(INSTALL_PKGCONFIG_DIR) $(INSTALL_CONFIG_DIR))
	install -m 755 $(LIBIMAGES_PROGS) $(DESTDIR)$(INSTALL_BIN_DIR)
	install -m 644 $(addprefix run/include/images/,$(LIBIMAGES_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/images
	install -m 644 run/lib/libucw-images.$(LS) $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/pkgconfig/libucw-images.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/$(CONFIG_DIR)/,$(LIBIMAGES_CONFIGS)) $(DESTDIR)$(INSTALL_CONFIG_DIR)
ifdef CONFIG_STATIC_PIC
	install -m 644 run/lib/libucw-images-pic.$(LS) $(DESTDIR)$(INSTALL_LIB_DIR)
endif

.PHONY: install-libucw-images
