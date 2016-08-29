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

#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

#include "packet_frame.h"

#include "frame_queue.h"


struct dns_frame_queue *
dns_frame_queue_create(size_t capacity, enum dns_frame_queue_on_full on_full)
{
    struct dns_frame_queue *q = (struct dns_frame_queue*) malloc(sizeof(struct dns_frame_queue));
    q->queue = (struct dns_packet_frame**) malloc(sizeof(struct dns_packet_frame *) * capacity);
    q->length = 0;
    q->start = 0;
    q->capacity = capacity;
    q->on_full = on_full;
    q->total_size = 0;
    pthread_cond_init(&q->empty_cond, NULL);
    pthread_cond_init(&q->full_cond, NULL);
    pthread_mutex_init(&q->mutex, NULL);
    return q;
}

void
dns_frame_queue_destroy(struct dns_frame_queue* q)
{
    assert(q);

    for (int i = 0; i < q->length; i++)
        dns_packet_frame_destroy(q->queue[i]);
    free(q->queue);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->full_cond);
    pthread_cond_destroy(&q->empty_cond);
    free(q);
}

static inline struct dns_packet_frame *
dns_frame_queue_dequeue_internal(struct dns_frame_queue* q)
{
    assert(q);

    struct dns_packet_frame *f = q->queue[q->start];
    q->start = (q->start + 1) % q->capacity;
    q->length --;
    q->total_size -= f->size;
    return f;
}

void
dns_frame_queue_enqueue(struct dns_frame_queue* q, struct dns_packet_frame *f)
{
    assert(f);

    if (q == NULL) {
        dns_packet_frame_destroy(f);
        return;
    }

    pthread_mutex_lock(&q->mutex);

    while (q->length + 1 > q->capacity) {
        assert(q->length >= 1);
        if (q->on_full == DNS_QUEUE_BLOCK) {
            pthread_cond_wait(&q->full_cond, &q->mutex);
        }
        if (q->on_full == DNS_QUEUE_DROP_OLDEST) {
            dns_packet_frame_destroy(dns_frame_queue_dequeue_internal(q));
        }
        if (q->on_full == DNS_QUEUE_DROP_NEWEST) {
            dns_packet_frame_destroy(f);
            f = NULL;
            break;
        }      
    }

    if (f) {
        q->queue[(q->start + q->length) % q->capacity] = f;
        q->length ++;
        q->total_size += f->size;
    }

    pthread_cond_broadcast(&q->empty_cond);
    pthread_mutex_unlock(&q->mutex);
}

struct dns_packet_frame *
dns_frame_queue_dequeue(struct dns_frame_queue* q)
{
    assert(q);

    pthread_mutex_lock(&q->mutex);

    while (q->length == 0) {
        pthread_cond_wait(&q->empty_cond, &q->mutex);
    }
    struct dns_packet_frame *f = dns_frame_queue_dequeue_internal(q);

    pthread_cond_broadcast(&q->full_cond);
    pthread_mutex_unlock(&q->mutex);
    return f;
}

