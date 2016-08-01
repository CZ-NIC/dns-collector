#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "packet_frame.h"
#include "frame_queue.h"
#include "worker_frame_logger.h"

struct dns_worker_frame_logger *
dns_worker_frame_logger_create(const char *name, struct dns_frame_queue *in, struct dns_frame_queue *out)
{
    assert(in);
    assert(name);
    struct dns_worker_frame_logger *l = xmalloc_zero(sizeof(struct dns_worker_frame_logger));
    l->in = in;
    l->out = out;
    l->name = strdup(name);
    pthread_mutex_init(&l->running, NULL);
    return l;
}

void
dns_worker_frame_logger_destroy(struct dns_worker_frame_logger *l)
{
    if (pthread_mutex_trylock(&l->running) != 0)
        die("destroying a running logger");
    pthread_mutex_unlock(&l->running);
    pthread_mutex_destroy(&l->running);
    free(l->name);
    free(l);
}

static void*
dns_worker_frame_logger_main(void *logger)
{
    struct dns_worker_frame_logger *l = logger;
    int run = 1;
    while(run) {
        struct dns_packet_frame *f = dns_frame_queue_dequeue(l->in);
        if (f->time_start == DNS_NO_TIME) {
            msg(L_INFO, "Frame (type %d) through %s: %ld packets, %lu bytes, no time info", f->type, l->name, f->count, f->size);
        } else {
            msg(L_INFO, "Frame (type %d) through %s: %ld packets, %lu bytes, %.3f - %.3f", f->type, l->name, f->count, f->size,
                dns_us_time_to_fsec(f->time_start), dns_us_time_to_fsec(f->time_end));
        }
        if (f->type == 1)
            run = 0;
        dns_frame_queue_enqueue(l->out, f);
    }
    pthread_mutex_unlock(&l->running);
    return NULL;
}

void
dns_worker_frame_logger_finish(struct dns_worker_frame_logger *l)
{
    int r = pthread_join(l->thread, NULL);
    assert(r == 0);
    msg(L_DEBUG, "Worker frame logger \"%s\" stopped and joined", l->name);
}

void
dns_worker_frame_logger_start(struct dns_worker_frame_logger *l)
{
    if (pthread_mutex_trylock(&l->running) != 0)
        die("starting a running logger");
    int r = pthread_create(&l->thread, NULL, dns_worker_frame_logger_main, l);
    assert(r == 0);
    msg(L_DEBUG, "Worker frame logger \"%s\" started", l->name);
}

