# Example Makefile for a stand-alone program using the libucw build system
# (c) 2007 Martin Mares <mj@ucw.cz>

# The default target
all: runtree programs

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

# Include makefiles of libraries we wish to use
include $(s)/ucw/Makefile
include $(s)/charset/Makefile
include $(s)/lang/Makefile
include $(s)/images/Makefile

# Programs we want to compile
PROGS+=$(o)/test
$(o)/test: $(o)/test.o $(LIBUCW) $(LIBLANG) $(LIBCHARSET) $(LIBIMAGES)

# All tests (%-t) get automatically linked with libucw
TESTING_DEPS=$(LIBUCW)

# And finally the default rules of the build system
include $(s)/build/Makebottom
