#ifndef DNSCOL_WORKER_PACKET_MATCHER_H
#define DNSCOL_WORKER_PACKET_MATCHER_H

#include "common.h"

struct dns_frame_queue;
struct dns_packet_hash;


/**
 * A worker matching requests to responses within a time window.
 * All the packets are kept in a queue, staying there for matching_duration.
 * All requests are hashed by IP+PORT+ID, all responses are matched against
 * this hash to find a matching QNAME (which may be empty in some cases).
 */
struct dns_worker_packet_matcher {
    /** Input and output queue. Output may be NULL (discard). */
    struct dns_frame_queue *in, *out;

    /** The thread processing this output. Owned by the output. */
    pthread_t thread;

    /** The mutex indicating that the thread is started and running. */
    pthread_mutex_t running;

    /** The hash table with packets */
    struct dns_packet_hash *hash_table;

    /** The hash table with packets */
    clist packet_queue;

    /** The length of the window for finding matches */
    dns_us_time_t matching_duration;

    /** Maximum packet frame duration */
    dns_us_time_t frame_max_duration;

    /** Maximum packet frame size in bytes */
    int frame_max_size;

    /** Currently read frame */
    struct dns_packet_frame *inframe;

    /** Currently written frame */
    struct dns_packet_frame *outframe;

    /** Time of the last packet read */
    dns_us_time_t current_time;
};

/** Default and minimal size for the matcher hash table */
#define WORKER_PACKET_MATCHER_MIN_HASH_SIZE 1024

/**
 * Create a packet matcher. The output queue is optional.
 */
struct dns_worker_packet_matcher *
dns_worker_packet_matcher_create(struct dns_config *conf, struct dns_frame_queue *in, struct dns_frame_queue *out);

/**
 * Wait for the packet matcher thread to stop.
 */
void
dns_worker_packet_matcher_finish(struct dns_worker_packet_matcher *pm);

/**
 * Destroy the packet matcher struct, the packet matcher thread must not be running!
 */
void
dns_worker_packet_matcher_destroy(struct dns_worker_packet_matcher *pm);

/**
 * Start the packet matcher thread. The thread must not be already running!
 */
void
dns_worker_packet_matcher_start(struct dns_worker_packet_matcher *pm);

#endif /* DNSCOL_WORKER_PACKET_MATCHER_H */
