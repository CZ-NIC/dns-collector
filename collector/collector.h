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

    pthread_mutex_t collector_mutex;
    pthread_cond_t output_cond;
    pthread_cond_t unblock_cond;

//    /** NULL-terminated array of configured outputs, later also with active output threads. */
//    struct dns_output *outputs[];

    /** DNS collector status and stats. \todo Redesign */
    dns_stats_t stats;

    /** Open input pcap or NULL. Owned by collector. */
    pcap_t *pcap;

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

/**
 * Run the collector processing loop. Process all the inputs,
 * then finalize the outputs.
 */
void
dns_collector_run(dns_collector_t *col);

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
 * Open a pcap file for reading.
 * Data link layer must be DLT_RAW. 
 */
dns_ret_t
dns_collector_open_pcap_file(dns_collector_t *col, const char *pcap_fname);


/* Packet processing ************************************************/

/**
 * Try to get the next packet from pcap and process it with
 * `dns_collector_process_packet`.
 *
 * \return `DNS_RET_ERR`, `DNS_RET_OK`, `DNS_RET_EOF` or `DNS_RET_TIMEOUT`; see `pcap_next_ex()`.
 */
dns_ret_t
dns_collector_next_packet(dns_collector_t *col);

/**
 * Process and dissect the packet. Create `dns_packet_t`, fill with data, parse it,
 * append to a frame, log or dump.
 */
void
dns_collector_process_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);


#endif /* DNSCOL_COLLECTOR_H */
