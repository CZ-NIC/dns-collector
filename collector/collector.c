
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "collector.h"
#include "writeproto.h"
#include "timeframe.h"
#include "packet.h"

dns_collector_t *
dns_collector_create(const dns_collector_config_t *conf)
{
    assert(conf);

    dns_collector_t *col = (dns_collector_t *)calloc(sizeof(dns_collector_t), 1);
    if (!col) return NULL;
  
    col->config = conf;

    col->pcap = pcap_open_dead(DLT_RAW, conf->dns_capture_limit + 128);
    // TODO: specify maximum extra IP6+TCP header size

    if (!col->pcap) {
        fprintf(stderr, "error: pcap_open_dead() failed\n");
        free(col);
        return NULL;
    }

    return col;
}


void
dns_collector_destroy(dns_collector_t *col)
{ 
    assert(col && col->pcap);
    
    if (col->tf_cur)
        dns_timeframe_destroy(col->tf_cur);
    if (col->tf_old)
        dns_timeframe_destroy(col->tf_old);
    dns_collector_dump_close(col);
    pcap_close(col->pcap);
   
    free(col);
}


dns_ret_t
dns_collector_open_pcap_file(dns_collector_t *col, const char *pcap_fname)
{
    char pcap_errbuf[PCAP_ERRBUF_SIZE];

    assert(col && col->pcap && pcap_fname);

    pcap_t *newcap = pcap_open_offline(pcap_fname, pcap_errbuf);

    if (!newcap) {
        fprintf(stderr, "libpcap error: %s\n", pcap_errbuf);
        return DNS_RET_ERR;
    }

    if (pcap_datalink(newcap) != DLT_RAW) {
        fprintf(stderr, "error: pcap with link %s not supported (only DLT_RAW)\n", pcap_datalink_val_to_name(pcap_datalink(newcap)));
        pcap_close(newcap);
        return DNS_RET_ERR;
    }

    pcap_close(col->pcap);
    col->pcap = newcap;
    return DNS_RET_OK;
}


dns_ret_t
dns_collector_dump_open(dns_collector_t *col, const char *dump_fname)
{
    assert(col && dump_fname && col->pcap);

    dns_collector_dump_close(col);
    col->pcap_dump = pcap_dump_open(col->pcap, dump_fname);

    if (!col->pcap_dump) {
        fprintf(stderr, "libpcap error: %s\n", pcap_geterr(col->pcap));
        return DNS_RET_ERR;
    }

    return DNS_RET_OK;
}

void
dns_collector_dump_close(dns_collector_t *col)
{
    assert(col && col->pcap);
    
    if (col->pcap_dump) {
        pcap_dump_close(col->pcap_dump);
        col->pcap_dump = NULL;
    }
}

dns_ret_t
dns_collector_next_packet(dns_collector_t *col)
{
    int r;
    struct pcap_pkthdr *pkt_header;
    const u_char *pkt_data;

    assert(col && col->pcap);

    r = pcap_next_ex(col->pcap, &pkt_header, &pkt_data);

    switch(r) {
        case -2: 
            return DNS_RET_EOF;

        case -1:
            fprintf(stderr, "libpcap error: %s\n", pcap_geterr(col->pcap));
            return DNS_RET_ERR;

        case 0:
            return DNS_RET_TIMEOUT;

        case 1:
            col->stats.packets_captured++;
            if (col->tf_cur)
                col->tf_cur->stats.packets_captured++;

            dns_us_time_t now = dns_us_time_from_timeval(&(pkt_header->ts));
            if ((!col->tf_cur) || (col->tf_cur->time_start + col->config->frame_length < now))
                dns_collector_rotate_frames(col, now);

            dns_collector_process_packet(col, pkt_header, pkt_data);

            return DNS_RET_OK;
    }

    assert(0);
}

void
dns_collector_process_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    assert(col && pkt_header && pkt_data && col->tf_cur);

    dns_packet_t *pkt = dns_packet_create();
    assert(pkt);

    dns_packet_from_pcap(col, pkt, pkt_header, pkt_data);
    dns_ret_t r = dns_packet_parse(col, pkt);
    if (r == DNS_RET_DROPPED) {
        free(pkt);
        return;
    }

    dns_packet_create_hash_key(col, pkt);

    dns_timeframe_add_packet(col->tf_cur, pkt);
}

void
dns_collector_rotate_frames(dns_collector_t *col, dns_us_time_t time_now)
{
    if (col->tf_old) {
        dns_timeframe_writeout(col->tf_old, stdout); // TODO: specify file
        dns_timeframe_destroy(col->tf_old);
        col->tf_old = NULL;
    }

    if (time_now == 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        time_now = dns_us_time_from_timespec(&now);
    }

    if (col->tf_cur) {
        col->tf_cur -> time_end = time_now - 1; // prevent overlaps
        col->tf_old = col->tf_cur;
        col->tf_cur = NULL;
    }

    col->tf_cur = dns_timeframe_create(col, time_now);
}
