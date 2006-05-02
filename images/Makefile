# Testing dir... code will be moved somewhere else... maybe to trash :-)

DIRS+=images

PROGS+=$(addprefix $(o)/images/,image-idx image-test decomp)

$(o)/images/image-sig.o $(o)/images/image-sig.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/image-idx.o $(o)/images/image-idx.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/image-obj.o $(o)/images/image-obj.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/image-idx: $(o)/images/image-idx.o $(o)/images/image-obj.o $(o)/images/dup-cmp.o $(o)/indexer/iconfig.o $(o)/images/image-sig.o $(o)/images/kd-tree.o $(o)/images/color.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
$(o)/images/image-idx: LIBS+=-lGraphicsMagick -ljpeg -lpng
$(o)/images/image-test: $(o)/images/image-test.o $(o)/images/kd-tree.o $(LIBSH)

$(o)/images/color-t: LIBS+=-lm

TESTS+=$(addprefix $(o)/images/,color.test)

$(o)/images/color.test: $(o)/images/color-t

# By :;DF
$(o)/images/block_info.o $(o)/images/block_info.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/k_means.o $(o)/images/k_means.oo: CFLAGS+=-I/usr/include/GraphicsMagick
$(o)/images/decomp.o $(o)/images/decomp.oo: CFLAGS+=-I/usr/include/GraphicsMagick

$(o)/images/decomp: $(o)/images/decomp.o $(o)/images/block_info.o $(o)/images/k_means.o $(LIBSH) $(LIBLANG) $(LIBCHARSET)
$(o)/images/decomp: LIBS+=-lGraphicsMagick -ljpeg -lm

