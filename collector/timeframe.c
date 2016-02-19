
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "timeframe.h"
#include "packet.h"
#include "output.h"


dns_timeframe_t *
dns_timeframe_create(dns_collector_t *col, dns_us_time_t time_start) 
{
    assert(col);

    dns_timeframe_t *frame = (dns_timeframe_t*) xmalloc_zero(sizeof(dns_timeframe_t));
    frame->collector = col;

    // Init times
    if (time_start != DNS_NO_TIME) {
        frame->time_start = time_start;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        frame->time_start = dns_us_time_from_timespec(&now);
    }
    frame->time_end = frame->time_start + col->config->timeframe_length;

    // Init packet sequence
    frame->packets_next_ptr = &(frame->packets);

    // Init hash
    frame->hash_order = col->config->hash_order; 
    frame->hash_param = random_u64();
    // Make sure the modulo is larger than hash size, but not more than 8x
    frame->hash_param |= 1 << frame->hash_order;
    frame->hash_param &= (1 << (frame->hash_order + 3)) - 1;

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
    dns_packet_t *pkt = frame->packets;

    while(pkt) {
        CLIST_FOR_EACH(struct dns_output*, out, frame->collector->config->outputs) {

            dns_output_check_rotation(out, pkt->ts);

            if (out->write_packet)
                out->write_packet(out, pkt);
        }

        pkt = pkt -> next_in_timeframe;
    }

    msg(L_INFO, "Frame %lf - %lf wrote %d queries",
            dns_us_time_to_fsec(frame->time_start), dns_us_time_to_fsec(frame->time_end),
            frame->packets_count);
}





