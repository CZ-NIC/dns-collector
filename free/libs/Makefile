# Makefile for the stand-alone release of Sherlock libraries
# (c) 2007 Martin Mares <mj@ucw.cz>

# The default target
all: runtree libs

# Include configuration
s=.
-include obj/config.mk
obj/config.mk:
	@echo "You need to run configure first." && false

# We will use the libucw build system
include $(s)/build/Maketop

# Set up names of common libraries (to avoid forward references in rules)
LIBLANG=$(o)/lang/liblang.pc
LIBCHARSET=$(o)/charset/libcharset.pc
LIBIMAGES=$(o)/images/libimages.pc
LIBSH=$(o)/sherlock/libsh.pc

# Include makefiles of libraries we wish to use
include $(s)/lib/Makefile
include $(s)/charset/Makefile
include $(s)/lang/Makefile
include $(s)/images/Makefile
include $(s)/sherlock/Makefile

libs: $(LIBUCW) $(LIBSH) $(LIBIMAGES) $(LIBCHARSET) $(LIBLANG)

# And finally the default rules of the build system
include $(s)/build/Makebottom
