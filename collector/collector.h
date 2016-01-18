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
    const char *dumpfile_base;
};


/** Stats of a DNS collector */
struct dns_collector_stats {
    /** all packets received from pcap_next_ex(). */
    long packets_captured;
    /** exceptional packets (some/all of them dumped). */
    long packets_exceptional;
    /** exceptional dumped packets. */
    long packets_dumped;
};


/** DNS collector instance */
struct dns_collector {

    /** configuration variables. Not owned by collector. */
    const dns_collector_config_t *config;

    /** dns collector status and stats. */
    dns_collector_stats_t stats;

    /** open pcap. Owned by collector. May be NULL. */
    pcap_t *pcap;

    /** dumper for unprocessed packets. Owned by collector. May be NULL. */
    pcap_dumper_t *pcap_dumper;

    /** name of the current dumper file.
     * Empty (pcap_dumper_fname==0) only if pcap_dumper==NULL.
     * When pcap_dumper==NULL, may contain name of previously open file. */
    char pcap_dumper_fname[DNSCOL_MAX_FNAME_LEN];

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
 * Also opens a exceptional dump file (if configured).
 * If a pcap is currently open, it is closed and a new pcap is created.
 * The old and new pcap must be compatible (e.g. same network layer).
 *
 * If a pcap dump file is open, it has to be closed (as it is tied to the
 * previous pcap) and is reopened with the new pcap.
 */
int
dns_collector_open_pcap(dns_collector_t *col, const char *pcap_fname);

/**
 * Dump a packet to the exceptional packet pcap.
 *
 * Increases stat dump counter for both collector and current timeframe.
 * Return -1 on error, 0 otherwise.
 */
int
dns_collector_dump_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);

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
