#ifndef DNSCOL_PACKET_FRAME_H
#define DNSCOL_PACKET_FRAME_H

#include "common.h"

/**
 * A group of packets (or packet pairs) ordered by timestamp.
 */
struct dns_packet_frame {

    /** Start timestamp. */
    dns_us_time_t time_start;

    /** End timestamp. */
    dns_us_time_t time_end;

    /** Linked list of requests (some with responses) and responses without requests.
     * All owned by the frame. */
    clist packets;

    /** Number of queries in list (matched pairs counted as 1) = length of `packets` list. */
    size_t count;

    /** Size of the contained data (for memory limiting) */
    size_t size;
};

/**
 * Allocate and init the frame.
 */
dns_packet_frame_t *
dns_packet_frame_create(dns_us_time_t time_start, dns_us_time_t time_end);

/**
 * Destroy the frame and all inserted packets (and their responses).
 */
void
dns_packet_frame_destroy(dns_packet_frame_t *frame);

/**
 * Append the given packet to the timeframe sequence, taking ownership.
 */
void
dns_packet_frame_append_packet(dns_packet_frame_t *frame, dns_packet_t *pkt);

#endif /* DNSCOL_PACKET_FRAME_H */
