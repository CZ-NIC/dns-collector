/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "packet_frame.h"
#include "packet.h"
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
        int requests=0, requests_matched=0, responses_unmatched=0;
        CLIST_FOR_EACH(struct dns_packet *, pkt, f->packets) {
            if (DNS_PACKET_IS_REQUEST(pkt)) {
                requests++;
                if (pkt->response)
                    requests_matched++;
            } else {
                responses_unmatched++;
            }
        }
        msg(L_DEBUG, "Frame details: %d requests (%d of that with response), %d unmatched responses",
            requests, requests_matched, responses_unmatched);
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

