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
    frame->count ++;
    frame->size += pkt->memory_size;
}

