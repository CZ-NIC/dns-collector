#ifndef DNSCOL_PACKET_FRAME_LOGGER_H
#define DNSCOL_PACKET_FRAME_LOGGER_H

#include "common.h"

struct dns_frame_queue;

struct dns_packet_frame_logger {
    /** Input and output queue. Output may be NULL (discard). */
    struct dns_frame_queue *in, *out;
    /** Name for logging, owned */
    char *name;
    /** The thread processing this output. Owned by the output. */
    pthread_t thread;
    /** The mutex indicating that the thread is started and running. */
    pthread_mutex_t running;
};

struct dns_packet_frame_logger *
dns_packet_frame_logger_create(const char *name, struct dns_frame_queue *in, struct dns_frame_queue *out)
{
    assert(in);
    assert(name);
    struct dns_packet_frame_logger *l = xmalloc_zero(sizeof(struct dns_packet_frame_logger));
    l->in = in;
    l->out = out;
    l->name = strdup(name);
    pthread_mutex_init(&l->running, NULL);
    return l;
}

void
dns_packet_frame_logger_destroy(struct dns_packet_frame_logger *l)
{
    if (pthread_mutex_trylock(&l->running) != 0)
        die("destroying a running logger");
    pthread_mutex_unlock(&l->running);
    pthread_mutex_destroy(&l->running);
    free(l->name);
    free(l);
}

void
dns_packet_frame_logger_start(struct dns_packet_frame_logger *l)
{
    if (pthread_mutex_trylock(&l->running) != 0)
        die("starting a running logger");
    pthread_create(l->thread, NULL, dns_packet_frame_logger_main, l);
}

static void
dns_packet_frame_logger_main(void *logger)
{
    struct dns_packet_frame_logger *l = logger;
    int run = 1;
    while(run) {
        struct dns_packet_frame *f = dns_frame_queue_dequeue(l->in);
        msg(L_INFO, "Frame through %s: %d packets, %d bytes, %f - %f", l->name, f->count, f->size,
            dns_us_time_to_fsec(f->time_start), dns_us_time_to_fsec(f->time_end));
        if (f->type == 1) run = 0;
        if (l->out) {
            dns_frame_queue_enqueue(l->out, f);
        } else {
            dns_packet_frame_destroy(f);
        }
    }
}


#endif /* DNSCOL_PACKET_FRAME_LOGGER_H */