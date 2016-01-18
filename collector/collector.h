#ifndef DNSCOL_COLLECTOR_H
#define DNSCOL_COLLECTOR_H

#include <pcap/pcap.h>
#include <time.h>

#define DNSCOL_MAX_FNAME_LEN 256

/** Configuration of a DNS collector. */
struct dns_collector_config {
    const char *output_base;
    int active_frames;
    struct timespec frame_length;
    const char *dumpfile_base;
};
typedef struct dns_collector_config dns_collector_config_t;


/** Stats of a DNS collector */
struct dns_collector_stats {
    /** all packets received from pcap_next_ex(). */
    long packets_captured;
    /** exceptional packets (some/all of them dumped). */
    long packets_exceptional;
};
typedef struct dns_collector_stats dns_collector_stats_t;


/** DNS collector instance */
struct dns_collector {

    /** configuration variables. Not owned by collector. */
    dns_collector_config_t *config;

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
typedef struct dns_collector dns_collector_t;


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

#endif /* DNSCOL_COLLECTOR_H */
