#included from ../Makefile
here=./src

DEPS=$(wildcard $(here)/*.h)

SRCS=$(here)/common.c $(here)/input.c $(here)/frame_queue.c $(here)/packet_frame.c \
     $(here)/worker_frame_logger.c $(here)/main.c $(here)/dump.c $(here)/output.c \
     $(here)/output_csv.c $(here)/packet.c $(here)/worker_packet_matcher.c \
     $(here)/packet_hash.c $(here)/config.c

OBJS=$(sort $(SRCS:.c=.o))

clean::
	rm -f $(OBJS) $(PROG)

%.o: %.c $(DEPS) libucw
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS) libucw
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)


