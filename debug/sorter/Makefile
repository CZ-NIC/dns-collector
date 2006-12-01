# Testing the new sorter

DIRS+=debug/sorter
PROGS+=$(addprefix $(o)/debug/sorter/,retros file-test asio-test)

$(o)/debug/sorter/retros: $(o)/debug/sorter/retros.o $(LIBSH)
$(o)/debug/sorter/file-test: $(o)/debug/sorter/file-test.o $(LIBSH)
$(o)/debug/sorter/asio-test: $(o)/debug/sorter/asio-test.o $(LIBSH)
$(o)/debug/sorter/asio-test: LIBS+=-lpthread
