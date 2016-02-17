#ifndef DNSCOL_COLLECTOR_H
#define DNSCOL_COLLECTOR_H

#include <pcap/pcap.h>
#include <time.h>

#include "common.h"
#include "config.h"
#include "stats.h"

/** DNS collector instance */
struct dns_collector {

    /** configuration variables. Not owned by collector. */
    struct dns_config *config;

    /** dns collector status and stats. */
    dns_stats_t stats;

    /** open pcap. Owned by collector, should always be open,
     * initially may be dead (dummy) pcap. */
    pcap_t *pcap;

    /** dumper for unprocessed packets. Owned by collector. May be NULL. */
    pcap_dumper_t *pcap_dump;

    /** current and old timeframes.
     * Frames here are owned by the collector. */
    dns_timeframe_t *tf_cur;
    dns_timeframe_t *tf_old;
};


/**
 * Allocate new collector instance.
 *
 * All fields are zeroed.
 */
dns_collector_t *
dns_collector_create(struct dns_config *conf);

/**
 * Run the collector processing loop: process all the inputs,
 * then finalize the outputs.
 */
void
collector_run(dns_collector_t *col);

/**
 * Close and deallocate a collector instance and dependent
 * structures.
 */
void
dns_collector_destroy(dns_collector_t *col);

/**
 * Open a pcap file for reading.
 *
 * Data link layer must be DLT_RAW. 
 * Preserves open exceptional dump file.
 */
dns_ret_t
dns_collector_open_pcap_file(dns_collector_t *col, const char *pcap_fname);


/* Dumping exceptional packets **************************************/

/**
 * Open a dump file, closing one if already open.
 */
dns_ret_t
dns_collector_dump_open(dns_collector_t *col, const char *dump_fname);

/**
 *  Closes a dump file, NOP if none open.
 */
void
dns_collector_dump_close(dns_collector_t *col);


/* Packet processing ************************************************/

/**
 * Try to get the next packet from pcap and process it with
 * dns_collector_process_packet.
 *
 * Returns DNS_RET_ERR, DNS_RET_OK, DNS_RET_EOF or DNS_RET_TIMEOUT; see pcap_next_ex().
 */
dns_ret_t
dns_collector_next_packet(dns_collector_t *col);

/**
 * Process and dissect the packet: create dns_packet_t, file it to a frame, log or dump.
 */
void
dns_collector_process_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);

void
dns_collector_rotate_frames(dns_collector_t *col, dns_us_time_t time_now);


#endif /* DNSCOL_COLLECTOR_H */
