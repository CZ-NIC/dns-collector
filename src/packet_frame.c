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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "packet_frame.h"
#include "packet.h"

struct dns_packet_frame *
dns_packet_frame_create(dns_us_time_t time_start, dns_us_time_t time_end)
{
    struct dns_packet_frame *frame = xmalloc_zero(sizeof(struct dns_packet_frame));
    frame->time_start = time_start;
    frame->time_end = time_end;
    clist_init(&frame->packets);
    frame->count = 0;
    frame->size = 0;
    frame->type = 0;
    return frame;
}


struct dns_packet_frame *
dns_packet_frame_create_final(dns_us_time_t time)
{
    struct dns_packet_frame *frame = dns_packet_frame_create(time, time);
    frame->type = 1;
    return frame;
}

void
dns_packet_frame_destroy(struct dns_packet_frame *frame)
{
    void *tmp;
    CLIST_FOR_EACH_DELSAFE(struct dns_packet *, pkt, frame->packets, tmp) {
        dns_packet_destroy(pkt);
    }
    free(frame);
}

void
dns_packet_frame_append_packet(struct dns_packet_frame *frame, dns_packet_t *pkt)
{
    clist_add_tail(&frame->packets, &pkt->node);
    if (frame->time_end == DNS_NO_TIME || frame->time_end < pkt->ts)
        frame->time_end = pkt->ts;
    frame->count ++;
    frame->size += pkt->memory_size;
}

