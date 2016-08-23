#ifndef DNSCOL_PACKET_FRAME_LOGGER_H
#define DNSCOL_PACKET_FRAME_LOGGER_H

#include "common.h"

/**
 * \file worker_frame_logger.h
 * Example worker reporting the passing frames. Debugging only, not used anymore.
 */

struct dns_frame_queue;

/**
 * A "logger" worker structure passing packets from one queue to another.
 * Mostly for debugging.
 */
struct dns_worker_frame_logger {
    /** Input and output queue. Output may be NULL (discard). */
    struct dns_frame_queue *in, *out;
    /** Name for logging, owned */
    char *name;
    /** The thread processing this output. Owned by the output. */
    pthread_t thread;
    /** The mutex indicating that the thread is started and running. */
    pthread_mutex_t running;
};

/**
 * Create a logger. The output queue is optional.
 */
struct dns_worker_frame_logger *
dns_worker_frame_logger_create(const char *name, struct dns_frame_queue *in, struct dns_frame_queue *out);

/**
 * Wait for a logger thread to stop.
 */
void
dns_worker_frame_logger_finish(struct dns_worker_frame_logger *l);

/**
 * Destroy a logger. The logger must not be running!
 */
void
dns_worker_frame_logger_destroy(struct dns_worker_frame_logger *l);

/**
 * Start the logger thread. The logger must not be already running!
 */
void
dns_worker_frame_logger_start(struct dns_worker_frame_logger *l);

#endif /* DNSCOL_PACKET_FRAME_LOGGER_H */
