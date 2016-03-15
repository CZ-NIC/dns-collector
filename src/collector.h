#ifndef DNSCOL_COLLECTOR_H
#define DNSCOL_COLLECTOR_H

/**
 * \file collector.h
 * DNS collector instance definition.
 */

#include <pcap/pcap.h>
#include <time.h>

#include "common.h"
#include "config.h"
#include "stats.h"

#define DNS_MAX_OUTPUTS 64

/** DNS collector instance. */
struct dns_collector {

    /** Configuration variables. Not owned by collector. */
    struct dns_config *conf;

    /** Collector-wide mutex for shared data.
     * Is only un/locked by every thread limited number of times per frame (not per packet!).
     * Should be very low-overhead and held only for the shared-data access. */
    pthread_mutex_t collector_mutex;
    /** Condition signalling to the outputs that a new frame might be available in their queue */
    pthread_cond_t output_cond;
    /** Condition signalling to the collector that a frame got processed
     * (and a next one *might* be pushed without blocking). */
    pthread_cond_t unblock_cond;

    /** DNS collector status and stats. \todo Redesign */
    dns_stats_t stats;

    /** Current timeframe. Owned by the collector. */
    dns_timeframe_t *tf_cur;

    /** Previous timeframe. Owned by the collector. */
    dns_timeframe_t *tf_old;
};


/**
 * Create new collector instance with given config. 
 */
dns_collector_t *
dns_collector_create(struct dns_config *conf);

/**
 * Create one thread per output and start it.
 */
void
dns_collector_start_output_threads(dns_collector_t *col);

/**
 * Set given stop flag for all output threads and join() them.
 */
void
dns_collector_stop_output_threads(dns_collector_t *col, enum dns_output_stop how);

void
dns_collector_run_on_pcap(dns_collector_t *col, char *inuri);

/**
 * Run the collector processing loop. Process all data from the inputs.
 */
void
dns_collector_run_on_inputs(dns_collector_t *col, clist *inputs, int offline);

void
dns_collector_update_rotation(dns_collector_t *col, dns_us_time_t time);



/**
 * Rotate the timeframes.
 * Writeout and destroy `tf_old` (if any), move `tf_cut` to `tf_old` (if any), create new `tf_cur`.
 */
void
dns_collector_rotate_frames(dns_collector_t *col, dns_us_time_t time_now);


/**
 * Flush the remaining frames into the outputs.
 */
void
dns_collector_finish(dns_collector_t *col);

/**
 * Push the given timeframe to all outputs. Takes timeframe ownership.
 * Deals with the collector mutex (should be unlocked when called).
 */
void
dns_collector_output_timeframe(struct dns_collector *col, struct dns_timeframe *tf);

/**
 * Close and deallocate a collector instance and dependent
 * structures. Close configured outputs.
 */
void
dns_collector_destroy(dns_collector_t *col);

/**
 * Process and dissect the packet. Create `dns_packet_t`, fill with data, parse it,
 * append to a frame, log or dump.
 */
void
dns_collector_process_packet(dns_collector_t *col, libtrace_packet_t *packet);


#endif /* DNSCOL_COLLECTOR_H */
