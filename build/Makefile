# Makefile for Sherlock Build Tools

DIRS+=build

$(o)/build/genhash: $(o)/build/genhash.o

# This is a hack which compensates make's desires for propagating per-rule settings
# of variables: if some module specifies its own LIBS and it depends on genhash,
# genhash is sometimes built with the module's LIBS (if it isn't already built).
# A proper solution would be using a different rule for linking build/*, but
# as it currently concerns only genhash, it's easier to battle this way.
$(o)/build/genhash: LIBS=
