# Example Makefile for a stand-alone program using the libucw build system
# (c) 2007 Martin Mares <mj@ucw.cz>
# (c) 2008 Michal Vaner <vorner@ucw.cz>

# The default target
all: runtree programs

# Include configuration
s=.
-include obj/config.mk
obj/config.mk:
	@echo "You need to run configure first." && false

# Do not show strange errors if the BUILDSYS is not set
# (means noone yet called configure and it reported above)
ifdef BUILDSYS

# We will use the libucw build system
include $(BUILDSYS)/Maketop

# Add the detected flags to all the global flags
CFLAGS+=$(LIBUCW_CFLAGS)
LIBS+=$(LIBUCW_LIBS)

# Programs we want to compile
PROGS+=$(o)/test

# And how they are created
$(o)/test: $(o)/test.o $(LIBUCW)

# And finally the default rules of the build system
include $(BUILDSYS)/Makebottom

endif
