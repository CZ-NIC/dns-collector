# Testing dir... code will be moved somewhere else... maybe to trash :-)

DIRS+=images

PROGS+=$(addprefix $(o)/images/,image-idx image-test)

$(o)/images/image-sig.o $(o)/images/image-sig.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/image-idx.o $(o)/images/image-idx.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/image-idx: $(o)/images/images.o $(o)/images/image-idx.o $(o)/indexer/iconfig.o $(o)/images/image-sig.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
$(o)/images/image-idx: LIBS+=-lGraphicsMagick
$(o)/images/image-test: $(o)/images/images.o $(o)/images/image-test.o $(LIBSH)

