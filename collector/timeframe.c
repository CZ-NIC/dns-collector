
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

    dns_timeframe_t *frame = (dns_timeframe_t*) xmalloc_zero(sizeof(dns_timeframe_t));
    frame->collector = col;

    // Init times
    if (time_start >= 0) {
        frame->time_start = time_start;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        frame->time_start = dns_us_time_from_timespec(&now);
    }
    frame->time_end = frame->time_start + col->config->frame_length;

    // Init packet sequence
    frame->packets_next_ptr = &(frame->packets);

    // Init hash
    // TODO: configurable order
    frame->hash_order = 20; 
    // Account for possibly small RAND_MAX
    frame->hash_param = rand() + (rand() << 16);
    // Make sure the modulo is larger than hash size, but not more than twice
    frame->hash_param |= 1 << frame->hash_order;
    frame->hash_param &= (1 << (frame->hash_order + 1)) - 1;

    frame->hash_data = (dns_packet_t **) xmalloc_zero(sizeof(dns_packet_t *) * DNS_TIMEFRAME_HASH_SIZE(frame->hash_order));

    return frame;
}

void
dns_timeframe_destroy(dns_timeframe_t *frame) 
{
    dns_packet_t *p = frame->packets, *ptmp;

    // Free hash
    xfree(frame->hash_data);

    // Free packet sequence
    while(p) {
        ptmp = p;
        p = p -> next_in_timeframe;
        free(ptmp);

        // also destroys pkt->response, if any
        dns_packet_destroy(ptmp);
    }

    free(frame);
}

void
dns_timeframe_append_packet(dns_timeframe_t *frame, dns_packet_t *pkt)
{
    assert(frame && pkt);

    // add to sequence
    pkt->next_in_timeframe = NULL;
    *(frame->packets_next_ptr) = pkt;
    frame->packets_next_ptr = &(pkt->next_in_timeframe);
    frame->packets_count++;

    // Add to hash if request
    if (DNS_PACKET_IS_REQUEST(pkt)) {
        // Add to request hash
        uint64_t hash = dns_packet_hash(pkt, frame->hash_param);
        hash = hash % DNS_TIMEFRAME_HASH_SIZE(frame->hash_order);
        pkt->next_in_hash = frame->hash_data[hash];
        frame->hash_data[hash] = pkt;
        frame->hash_elements++;
    }
}

dns_packet_t *
dns_timeframe_match_response(dns_timeframe_t *frame, dns_packet_t *pkt)
{
    assert(frame && pkt && DNS_PACKET_IS_RESPONSE(pkt));

    uint64_t hash = dns_packet_hash(pkt, frame->hash_param);
    hash = hash % DNS_TIMEFRAME_HASH_SIZE(frame->hash_order);
    dns_packet_t **pp = &(frame->hash_data[hash]);

    // Explore hash bucket
    while(*pp) {
        dns_packet_t *req = *pp;
        if (dns_packets_match(req, pkt)) {
            // Assign response to request
            assert(req->response == NULL);
            req->response = pkt;

            // Remove request from hash
            *pp = req->next_in_hash;
            frame->hash_elements--;

            return req;
        }
        pp = &((*pp)->next_in_hash);
    }

    return NULL;
}

void
dns_timeframe_writeout(dns_timeframe_t *frame, FILE *f)
{
    assert(frame && f);

    DnsQuery q;
    dns_packet_t *pkt = frame->packets;
    u_char buf[DNS_MAX_PROTO_LEN];
    size_t len;

    while(pkt) {
        if (DNS_PACKET_IS_REQUEST(pkt))
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

        pkt = pkt -> next_in_timeframe;
    }

    fprintf(stderr, "Frame %lf - %lf wrote %d queries\n",
            dns_us_time_to_sec(frame->time_start), dns_us_time_to_sec(frame->time_end),
            frame->packets_count);
}





