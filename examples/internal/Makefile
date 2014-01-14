# Example Makefile for a stand-alone program using the libucw build system
# (c) 2007 Martin Mares <mj@ucw.cz>

# The default target
all: runtree programs

# Include configuration
s=.
-include obj/config.mk
obj/config.mk:
	@echo "You need to run configure first." && false

BUILDSYS=$(s)/build

# We will use the libucw build system
include $(BUILDSYS)/Maketop

# Set up names of common libraries (to avoid forward references in rules)
LIBCHARSET=$(o)/charset/libucw-charset.pc
LIBIMAGES=$(o)/images/libucw-images.pc

# Include makefiles of libraries we wish to use
include $(s)/ucw/Makefile
include $(s)/charset/Makefile
include $(s)/images/Makefile

# Programs we want to compile
PROGS+=$(o)/test
$(o)/test: $(o)/test.o $(LIBUCW) $(LIBCHARSET) $(LIBIMAGES)

# All tests (%-t) get automatically linked with libucw
TESTING_DEPS=$(LIBUCW)

# And finally the default rules of the build system
include $(BUILDSYS)/Makebottom
