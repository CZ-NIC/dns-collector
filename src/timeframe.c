
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "timeframe.h"
#include "packet.h"
#include "output.h"


dns_timeframe_t *
dns_timeframe_create(struct dns_config *conf, dns_us_time_t time_start) 
{
    assert(conf);

    dns_timeframe_t *frame = (dns_timeframe_t*) xmalloc_zero(sizeof(dns_timeframe_t));

    // Init times
    if (time_start != DNS_NO_TIME) {
        frame->time_start = time_start;
    } else {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        frame->time_start = dns_us_time_from_timespec(&now);
    }
    frame->time_end = frame->time_start + conf->timeframe_length;

    // Init packet sequence
    frame->packets_next_ptr = &(frame->packets);

    // Init hash
    frame->hash_order = conf->hash_order; 
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

    xfree(frame);
}


void
dns_timeframe_decref(dns_timeframe_t *frame)
{
    assert(frame && (frame->refcount > 0));

    frame->refcount --;

    if (frame->refcount == 0)
        dns_timeframe_destroy(frame);
}


void
dns_timeframe_incref(dns_timeframe_t *frame)
{
    assert(frame && (frame->refcount >= 0));

    frame->refcount ++;
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



