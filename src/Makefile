.PHONY: all clean build-deps clean-deps help docs

# Configuration

# Set to gcc or clang
CC ?=gcc 

# Uncomment to use tcmalloc for faster allocations
#USE_TCMALLOC=1

# Customizing the path with build libucw
LIBUCW_DIR=../libs/libucw/

WARNS ?=-Wall -Wcast-align -Wunused-parameter -Wno-variadic-macros

ifeq ($(CC),clang)
WARNS+=-Wno-gnu-statement-expression -Wno-language-extension-token
endif

LDLIBS ?=
LDFLAGS ?=
CFLAGS ?=-O2 -g3 -pedantic
CFLAGS +=$(WARNS) -rdynamic -pthread -std=gnu11
LDLIBS +=-lknot -ltrace -lpthread

PROG=dnscol
DEPS=$(wildcard *.h)
SRCS=common.c input.c frame_queue.c packet_frame.c worker_frame_logger.c main.c dump.c \
     output.c output_csv.c packet.c worker_packet_matcher.c packet_hash.c config.c

ifdef USE_TCMALLOC
    CFLAGS+=-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    LDLIBS+=-ltcmalloc
endif

ifdef LIBUCW_DIR
    CFLAGS+=-I$(LIBUCW_DIR)/run/include/
    LDLIBS+=$(LIBUCW_DIR)/run/lib/libucw-6.5.a
endif

OBJS=$(sort $(SRCS:.c=.o))

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)


