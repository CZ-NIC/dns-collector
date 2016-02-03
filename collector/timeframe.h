#ifndef DNSCOL_TIMEFRAME_H
#define DNSCOL_TIMEFRAME_H

#include "common.h"
#include "stats.h"
#include "collector.h"

#include <time.h>

/**
 * Local structure for the packet linked list.
 */
struct dns_timeframe_elem {
    dns_packet_t *elem;
    struct dns_timeframe_elem *next;
};

/**
 * Single time frame.
 *
 * Unit to match requests and responses. Multiple frames may be written to a
 * single file, stats should be also file-based. (TODO)
 */
struct dns_timeframe {
    /** Owning collector */
    dns_collector_t *collector;

    /** Start timestamp */
    dns_us_time_t time_start;

    /** End timestamp */
    dns_us_time_t time_end;

    /** Local frame statistics
     * TODO: probably compute per file. */
    dns_stats_t stats;

    /** Linked list of requests (some with responses), and responses without requests.
     * Ordered by the arrival time, not necessarily by timestamps.
     * The dns_timeframe_elem's and packets are owned by the frame. */
    struct dns_timeframe_elem *packets;
    /** Pointer to the head of the list, pointer to be overwritten
     * with the new *dns_timeframe_elem. */
    struct dns_timeframe_elem **packets_next_elem_ptr;
    /** Number of queries (matched pairs counted as 1) = length of `packets` list. */
    uint32_t packets_count;

    // TODO: memory pool
    // TODO: query hash by (IP, PORT, DNS-ID, QNAME)
};

dns_timeframe_t *
dns_timeframe_create(dns_collector_t *col, dns_us_time_t time_start);

void
dns_timeframe_destroy(dns_timeframe_t *frame);

void
dns_timeframe_add_packet(dns_timeframe_t *frame, dns_packet_t *pkt);

void
dns_timeframe_writeout(dns_timeframe_t *frame, FILE *f);

#endif /* DNSCOL_TIMEFRAME_H */
