#ifndef DNSCOL_STATS_H
#define DNSCOL_STATS_H

#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "output.h"

// TODO: rework to otutputs somehow ...

/** Stats of a DNS collector or frame */
struct dns_stats {

    /** all packets received from pcap_next_ex(). */
    uint64_t packets_captured;
    
    /** Packets dropped for some reason */
    uint64_t packets_dropped;

    /** Packets dropped by reason */
    uint64_t packets_dropped_reason[dns_drop_LAST];

    /** Packets dumped for some reason, subset of dropped. */
    uint64_t packets_dumped;

    /** Packets dumped by reason, subset of dropped by reason. */
    uint64_t packets_dumped_reason[dns_drop_LAST];
};

struct dns_config;

/**
 * Print stats into a file. conf is optional (for dump configuration hints).
 */
void
dns_stats_fprint(const dns_stats_t *stats, const struct dns_config *conf, FILE *f);


#endif /* DNSCOL_STATS_H */

