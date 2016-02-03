#ifndef DNSCOL_TIMEFRAME_H
#define DNSCOL_TIMEFRAME_H

#include "common.h"
#include "stats.h"

#include <time.h>

/**
 * Local structure for the packet linked list.
 */
struct dns_timeframe_elem {
    dns_packet_t *elem;
    struct dns_timeframe_elem *next;
}

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

    /** Linked list of packet requests, and responses without requests.
     * Ordered by the arrival time, not necessarily by timestamps.
     * The dns_timeframe_elem's and packets are owned by the frame. */
    struct dns_timeframe_elem *packets;
    /** Pointer to the head of the list, pointer to be overwritten
     * with the new *dns_timeframe_elem. */
    struct dns_timeframe_elem **packets_next_elem_ptr;

    // TODO: memory pool
    // TODO: query hash by (IP, PORT, DNS-ID, QNAME)
};

#endif /* DNSCOL_TIMEFRAME_H */
