#ifndef DNSCOL_COLLECTOR_H
#define DNSCOL_COLLECTOR_H

#include <pcap/pcap.h>
#include <time.h>

#include "common.h"

/** Configuration of a DNS collector. */
struct dns_collector_config {
    const char *output_base;

    int active_frames;

    struct timespec frame_length;

    /* Dump dropped packets by reason */
    bool dump_packet_reason[dns_drop_LAST];
};

/** DNS collector instance */
struct dns_collector {

    /** configuration variables. Not owned by collector. */
    const dns_collector_config_t *config;

    /** dns collector status and stats. */
    dns_stats_t stats;

    /** open pcap. Owned by collector. May be NULL. */
    pcap_t *pcap;

    /** dumper for unprocessed packets. Owned by collector. May be NULL. */
    pcap_dumper_t *pcap_dump;

    /** active timeframes.
     * there are config->active_frames frames allocated. frames[0] is always
     * the current one, all timeframes except timeframes[0] may be NULL. 
     * Frames in the array are owned by the collector. */
    dns_timeframe_t *timeframes[];
};

/**
 * Allocate new collector instance.
 *
 * All fields are zeroed.
 */
dns_collector_t *
dns_collector_create(const dns_collector_config_t *conf);

/**
 * Close and deallocate a collector instance.
 */
void
dns_collector_destroy(dns_collector_t *col);

/**
 * Open a pcap file for reading.
 *
 * Data link layer must be DLT_RAW. 
 * Preserves open exceptional dump file.
 */
int
dns_collector_open_pcap_file(dns_collector_t *col, const char *pcap_fname);


/* Dumping exceptional packetc */

/**
 * Open a dump file, closing one if already open.
 */
int
dns_collector_dump_open(dns_collector_t *col, const char *dump_fname);

/**
 *  Closes a dump file, NOP if none open.
 */
void
dns_collector_dump_close(dns_collector_t *col);

/**
 * Dump a packet to the exceptional packet pcap.
 *
 * Increases stat dump counter for both collector and current timeframe.
 * Return -1 on error, 0 otherwise.
 */
int
dns_collector_dump_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);



/* Packet processing */

/**
 * Process and dissect the packet: file in to a frame, log or dump
 *
 * Returns -1 on error, 0 otherwise.
 */
int
dns_collector_process_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);

/**
 * Try to get the next packet from pcap and process it with
 * dns_collector_process_packet.
 *
 * Return -2 (eof), -1 (error), 0 (timeout) or 1 (ok) as pcap_next_ex.
 */
int
dns_collector_next_packet(dns_collector_t *col);


/**
 * Write current stats in a human-readable way.
 */
void
dns_collector_write_stats(dns_collector_t *col, FILE *f);


#endif /* DNSCOL_COLLECTOR_H */
