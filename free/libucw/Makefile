# Makefile for the stand-alone release of libucw
# (c) 2008 Martin Mares <mj@ucw.cz>

# The default target
all: runtree libs api programs extras configs

# Include configuration
s=.
-include obj/config.mk
obj/config.mk:
	@echo "You need to run configure first." && false

BUILDSYS=$(s)/build

# We will use the libucw build system
include $(BUILDSYS)/Maketop

# Install the build system
include $(BUILDSYS)/Makefile

# The UCW library
include $(s)/ucw/Makefile

# Build documentation by default?
ifdef CONFIG_DOC
all: docs
endif

libs: $(LIBUCW)

# And finally the default rules of the build system
include $(BUILDSYS)/Makebottom

ifndef CONFIG_LOCAL
install: all $(INSTALL_TARGETS)
else
install:
	@echo "Nothing to install, this is a local build." && false
endif
.PHONY: install
