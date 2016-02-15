#ifndef DNSCOL_TIMEFRAME_H
#define DNSCOL_TIMEFRAME_H

#include "common.h"
#include "stats.h"
#include "collector.h"

#include <time.h>

#define DNS_TIMEFRAME_HASH_SIZE(order) (1 << (order))

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
     * Linked by `packet->next_in_timeframe`.
     * Ordered by the arrival time, not necessarily by timestamps.
     * All owned by the frame. */
    dns_packet_t *packets;

    /** Pointer to the head of the list, pointer to be overwritten
     * with newly arriving packet. */
    dns_packet_t **packets_next_ptr;

    /** Number of queries in list (matched pairs counted as 1) = length of `packets` list. */
    uint32_t packets_count;

    /** Request hash by (client IP, transport, PORT, DNS-ID, QNAME) */
    uint32_t hash_order;
    uint64_t hash_elements;
    uint64_t hash_param;
    dns_packet_t **hash_data;


    // TODO: memory pool
};

/**
 * Allocate the timeframe and the hash table.
 * Set start time to `time_start` or the current time when `time_start==0`
 */
dns_timeframe_t *
dns_timeframe_create(dns_collector_t *col, dns_us_time_t time_start);

/**
 * Destroy the frame, hash and all inserted packets (and their responses).
 */
void
dns_timeframe_destroy(dns_timeframe_t *frame);

/**
 * Insert the packet to the timeframe sequence, taking ownership.
 * When `pkt` is request, it is also hashed to match with a response.
 * Call with response `pkt` only when no matching request was found,
 * these responses are just added to the sequence.
 */
void
dns_timeframe_append_packet(dns_timeframe_t *frame, dns_packet_t *pkt);

/**
 * Look for a matching request packet for `pkt` in the hash.
 * If found, takes ownership of `pkt` and assigns it as a response to the found request packet.
 * If found, returns the request packet (for information only), otherwise returns NULL.
 */
dns_packet_t *
dns_timeframe_match_response(dns_timeframe_t *frame, dns_packet_t *pkt);

/**
 * Write the frame protobufs to the given file.
 */
void
dns_timeframe_writeout(dns_timeframe_t *frame, FILE *f);

#endif /* DNSCOL_TIMEFRAME_H */
