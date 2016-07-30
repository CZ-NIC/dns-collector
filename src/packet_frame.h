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

    /** Frame type: 0 - packet load, 1 - final empty packet */
    int type;

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
struct dns_packet_frame *
dns_packet_frame_create(dns_us_time_t time_start, dns_us_time_t time_end);

/**
 * Allocate and init the final frame with given time.
 */
struct dns_packet_frame *
dns_packet_frame_create_final(dns_us_time_t time);

/**
 * Destroy the frame and all inserted packets (and their responses).
 */
void
dns_packet_frame_destroy(struct dns_packet_frame *frame);

/**
 * Append the given packet to the timeframe sequence, taking ownership.
 */
void
dns_packet_frame_append_packet(struct dns_packet_frame *frame, dns_packet_t *pkt);

#endif /* DNSCOL_PACKET_FRAME_H */
