# Tests related to the new sorter

DIRS+=ucw/sorter/debug
PROGS+=$(addprefix $(o)/ucw/sorter/debug/,radix-tune-bits radix-tune-thresh retros)

$(o)/ucw/sorter/debug/retros: $(o)/ucw/sorter/debug/retros.o $(LIBUCW)
$(o)/ucw/sorter/debug/radix-file-test: $(o)/ucw/sorter/debug/radix-file-test.o $(LIBUCW)
$(o)/ucw/sorter/debug/radix-asio-test: $(o)/ucw/sorter/debug/radix-asio-test.o $(LIBUCW)
$(o)/ucw/sorter/debug/radix-tune-bits: $(s)/ucw/sorter/debug/radix-tune-bits.sh
$(o)/ucw/sorter/debug/radix-tune-thresh: $(s)/ucw/sorter/debug/radix-tune-thresh.sh
