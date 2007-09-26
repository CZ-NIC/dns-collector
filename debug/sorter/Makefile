# Tests related to the new sorter

DIRS+=debug/sorter
PROGS+=$(addprefix $(o)/debug/sorter/,radix-file-test radix-asio-test radix-tune-bits radix-tune-thresh)

$(o)/debug/sorter/retros: $(o)/debug/sorter/retros.o $(LIBSH)
$(o)/debug/sorter/radix-file-test: $(o)/debug/sorter/radix-file-test.o $(LIBSH)
$(o)/debug/sorter/radix-asio-test: $(o)/debug/sorter/radix-asio-test.o $(LIBSH)
$(o)/debug/sorter/radix-tune-bits: $(s)/debug/sorter/radix-tune-bits.sh
$(o)/debug/sorter/radix-tune-thresh: $(s)/debug/sorter/radix-tune-thresh.sh
