# Makefile for the stand-alone release of Sherlock libraries
# (c) 2007 Martin Mares <mj@ucw.cz>

# The default target
all: runtree libs api programs extras configs

# Include configuration
s=.
-include obj/config.mk
obj/config.mk:
	@echo "You need to run configure first." && false

# We will use the libucw build system
include $(s)/build/Maketop

# Install config files
CONFIGS+=sherlock local

# Set up names of common libraries (to avoid forward references in rules)
LIBCHARSET=$(o)/charset/libcharset.pc
LIBSH=$(o)/sherlock/libsh.pc

# Include makefiles of libraries we wish to use
include $(s)/ucw/Makefile
include $(s)/charset/Makefile
include $(s)/sherlock/Makefile

ifdef CONFIG_LANG
LIBLANG=$(o)/lang/liblang.pc
include $(s)/lang/Makefile
endif

ifdef CONFIG_IMAGES
LIBIMAGES=$(o)/images/libimages.pc
include $(s)/images/Makefile
endif

libs: $(LIBUCW) $(LIBSH) $(LIBIMAGES) $(LIBCHARSET) $(LIBLANG)

# And finally the default rules of the build system
include $(s)/build/Makebottom
