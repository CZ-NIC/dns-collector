#ifndef DNSCOL_FRAME_QUEUE_H
#define DNSCOL_FRAME_QUEUE_H

/**
 * \file frame_queue.h
 * Thread-safe queue with bounded capacity and data size for packet_frames.
 */

#include <pthread.h>

#include "common.h"

enum dns_frame_queue_on_full {
    DNS_QUEUE_BLOCK = 0,
    DNS_QUEUE_DROP_NEWEST,
    DNS_QUEUE_DROP_OLDEST,
};

struct dns_packet_frame;

struct dns_frame_queue {
    /** Cirtcular array of queue items, queue[start] is oldest, queue[(start + length - 1) % capacity] newest. */
    struct dns_packet_frame **queue;
    /** Number of items in queue. */
    size_t length;
    /** Start of circular list. */
    size_t start;
    /** Maximum number of items in queue. */
    size_t capacity;

    /** Behavior on insert to full queue. */
    enum dns_frame_queue_on_full on_full;

    /** Size of the queued frames in bytes. */
    size_t total_size;
    /** Maximum allowed size in the queue. 0 for no size checks. Frame bigger that the limit is destroyed immediatelly. */
    size_t size_cap;

    pthread_cond_t empty_cond;
    pthread_cond_t full_cond;
    pthread_mutex_t mutex;
};

/**
 * Allocate a fixed-capacity queue and size bound.
 *
 * Size is counted in bytes as reported by the contained structures, size_cap == 0 ignores the cap.
 * The parameter on_full determines the behaviour on full queue, see enum dns_frame_queue_on_full.
 */
struct dns_frame_queue *
dns_frame_queue_create(size_t capacity, size_t size_cap, enum dns_frame_queue_on_full on_full);

/**
 * Free the queue and all contained frames.
 */
void
dns_frame_queue_destroy(struct dns_frame_queue* q);

/**
 * Enqueue a frame. Capacity and size bounds are handled based on on_full.
 * Blocks only on DNS_QUEUE_BLOCK.
 * q may be NULL, then the frame is destroyed.
 */
void
dns_frame_queue_enqueue(struct dns_frame_queue* q, struct dns_packet_frame *f);

/**
 * Dequeues the oldest frame, blocks on empty queue.
 */
struct dns_packet_frame *
dns_frame_queue_dequeue(struct dns_frame_queue* q);


#endif /* DNSCOL_FRAME_QUEUE_H */
