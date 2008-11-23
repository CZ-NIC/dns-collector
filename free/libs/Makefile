# Makefile for the stand-alone release of Sherlock libraries
# (c) 2007 Martin Mares <mj@ucw.cz>

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

# Set up names of common libraries (to avoid forward references in rules)
ifdef CONFIG_CHARSET
LIBCHARSET=$(o)/charset/libcharset.pc
endif
ifdef CONFIG_SHERLOCK_LIB
LIBSH=$(o)/sherlock/libsh.pc
LIBSHXML=$(o)/sherlock/xml/libshxml.pc
TESTING_DEPS=$(LIBSH)
else
TESTING_DEPS=$(LIBUCW)
endif

# The UCW library
include $(s)/ucw/Makefile

# Install config files
ifdef CONFIG_SHERLOCK_LIB
FREE_CONFIGS=sherlock local
CONFIGS+=$(FREE_CONFIGS)

INSTALL_TARGETS+=install-configs
install-configs:
	install -d -m 755 $(DESTDIR)$(INSTALL_CONFIG_DIR)
	install -m 644 $(addprefix run/$(CONFIG_DIR)/,$(FREE_CONFIGS)) $(DESTDIR)$(INSTALL_CONFIG_DIR)

include $(s)/sherlock/Makefile
endif

ifdef CONFIG_CHARSET
# Include makefiles of libraries we wish to use
include $(s)/charset/Makefile
endif

ifdef CONFIG_LANG
LIBLANG=$(o)/lang/liblang.pc
include $(s)/lang/Makefile
endif

ifdef CONFIG_IMAGES
LIBIMAGES=$(o)/images/libimages.pc
include $(s)/images/Makefile
endif

# Build documentation by default?
ifdef CONFIG_DOC
all: docs
endif

libs: $(LIBUCW) $(LIBSH) $(LIBIMAGES) $(LIBCHARSET) $(LIBLANG)

# And finally the default rules of the build system
include $(BUILDSYS)/Makebottom

ifndef CONFIG_LOCAL
install: all $(INSTALL_TARGETS)
else
install:
	@echo "Nothing to install, this is a local build." && false
endif
.PHONY: install
