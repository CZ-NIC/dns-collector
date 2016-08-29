/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DNSCOL_PACKET_FRAME_H
#define DNSCOL_PACKET_FRAME_H

#include "common.h"

/**
 * \file packet_frame.h
 * Packet container (short time interval).
 */

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
