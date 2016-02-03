
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "timeframe.h"
#include "writeproto.h"
#include "packet.h"


dns_timeframe_t *
dns_timeframe_create(dns_collector_t *col, dns_us_time_t time_start) 
{
    assert(col);

    dns_timeframe_t *frame = (dns_timeframe_t*) calloc(sizeof(dns_timeframe_t), 1);
    if (!frame)
        dns_die("Out of memory");

    if (time_start >= 0) {
        frame->time_start = time_start;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        frame->time_start = dns_us_time_from_timespec(&now);
    }
    frame->time_end = frame->time_start + col->config->frame_length;

    frame->collector = col;
    frame->packets_next_elem_ptr = &(frame->packets);

    return frame;
}

void
dns_timeframe_destroy(dns_timeframe_t *frame) 
{
    struct dns_timeframe_elem *p = frame->packets, *ptmp;

    while(p) {
        // also destroys pkt->response, if any
        dns_packet_destroy(p->elem);

        ptmp = p;
        p = p -> next;
        free(ptmp);
    }

    free(frame);
}

void
dns_timeframe_add_packet(dns_timeframe_t *frame, dns_packet_t *pkt)
{
    assert(frame && pkt);

    struct dns_timeframe_elem *e = calloc(sizeof(struct dns_timeframe_elem), 1);
    if (!e)
        dns_die("Out of memory");

    e->elem = pkt;
    *(frame->packets_next_elem_ptr) = e;
    frame->packets_next_elem_ptr = &(e->next);
}

void
dns_timeframe_writeout(dns_timeframe_t *frame, FILE *f)
{
    assert(frame);

    DnsQuery q;
    struct dns_timeframe_elem *p = frame->packets;
    u_char buf[DNS_MAX_PROTO_LEN];
    size_t len;

    while(p) {
        dns_packet_t *pkt = p->elem;

        if (DNS_HDR_FLAGS_QR(pkt->dns_data->flags) == 0)
            // request with optional response
            dns_fill_proto(frame->collector->config, pkt, pkt->response, &q);
        else
            // response only
            dns_fill_proto(frame->collector->config, NULL, pkt, &q);
        len = protobuf_c_message_pack((ProtobufCMessage *)&q, buf);
        if (len > sizeof(buf)) // Should never happen, but defensively:
            dns_die("Impossibly long protobuf");

        fwrite(&len, 2, 1, f);
        fwrite(buf, len, 1, f);

        p = p -> next;
    }
}





