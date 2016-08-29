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
