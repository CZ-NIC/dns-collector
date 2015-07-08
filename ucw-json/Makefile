# Makefile for the UCW JSON library
# (c) 2015 Martin Mares <mj@ucw.cz>

DIRS+=ucw-json
PROGS+=$(o)/ucw-json/json-test

LIBJSON_MODS=json parse format
LIBJSON_MOD_PATHS=$(addprefix $(o)/ucw-json/,$(LIBJSON_MODS))
LIBJSON_INCLUDES=json.h
LIBJSON_DEPS=$(LIBUCW)

$(o)/ucw-json/libucw-json$(LV).a: $(addsuffix .o,$(LIBJSON_MOD_PATHS))
$(o)/ucw-json/libucw-json$(LV).so: $(addsuffix .oo,$(LIBJSON_MOD_PATHS)) $(LIBJSON_DEPS)
$(o)/ucw-json/libucw-json$(LV).so: SONAME_SUFFIX=.0
$(o)/ucw-json/libucw-json.pc: $(LIBJSON_DEPS)

ifdef CONFIG_INSTALL_API
$(o)/ucw-json/libucw-json.pc: $(addprefix $(o)/ucw-json/libucw-json$(LV),.a .so)
endif

TESTS+=$(o)/ucw-json/json-test.test
$(o)/ucw-json/json-test: $(o)/ucw-json/json-test.o $(LIBJSON) $(LIBUCW)
$(o)/ucw-json/json-test.test: $(o)/ucw-json/json-test

API_LIBS+=libucw-json
API_INCLUDES+=$(o)/ucw-json/.include-stamp
$(o)/ucw-json/.include-stamp: $(addprefix $(s)/ucw-json/,$(LIBJSON_INCLUDES))
$(o)/ucw-json/.include-stamp: IDST=ucw-json
run/lib/pkgconfig/libucw-json.pc: $(o)/ucw-json/libucw-json.pc

INSTALL_TARGETS+=install-libucw-json-lib
install-libucw-json-lib:
	install -d -m 755 $(DESTDIR)$(INSTALL_LIB_DIR)
	install -m 644 run/lib/libucw-json$(LV).so.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-json$(LV).so.0.0
	ln -sf libucw-json$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-json$(LV).so.0
.PHONY: install-libucw-json-lib

INSTALL_TARGETS+=install-libucw-json-api
install-libucw-json-api:
	install -d -m 755 $(DESTDIR)$(INSTALL_INCLUDE_DIR)/ucw-json $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 run/lib/pkgconfig/libucw-json.pc $(DESTDIR)$(INSTALL_PKGCONFIG_DIR)
	install -m 644 $(addprefix run/include/ucw-json/,$(LIBJSON_INCLUDES)) $(DESTDIR)$(INSTALL_INCLUDE_DIR)/ucw-json
	ln -sf libucw-json$(LV).so.0.0 $(DESTDIR)$(INSTALL_LIB_DIR)/libucw-json$(LV).so
	install -m 644 run/lib/libucw-json$(LV).a $(DESTDIR)$(INSTALL_LIB_DIR)
.PHONY: install-libucw-json-api

include $(s)/ucw-json/doc/Makefile
