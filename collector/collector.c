
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "collector.h"
#include "timeframe.h"
#include "packet.h"

dns_collector_t *
dns_collector_create(struct dns_config *conf)
{
    assert(conf);

    dns_collector_t *col = (dns_collector_t *)xmalloc_zero(sizeof(dns_collector_t));

    pthread_mutex_init(&(col->collector_mutex), NULL);
    pthread_cond_init(&(col->output_cond), NULL);
    pthread_cond_init(&(col->unblock_cond), NULL);
  
    col->conf = conf;
    CLIST_FOR_EACH(struct dns_output*, out, conf->outputs) {
        dns_output_init(out, col);
    }
    
    return col;
}


void
collector_start_output_threads(dns_collector_t *col)
{
    pthread_mutex_lock(&(col->collector_mutex));
    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        int r = pthread_create(&(out->thread), NULL, dns_output_thread_main, out);
        if (r)
            die("Thread creation failed [err %d].", r);
    }
    pthread_mutex_unlock(&(col->collector_mutex));
}

void
collector_stop_output_threads(dns_collector_t *col, enum dns_output_stop how)
{
    assert(col && how != dns_os_none);

    pthread_mutex_lock(&(col->collector_mutex));
    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        out->stop_flag = how;
    }
    pthread_mutex_unlock(&col->collector_mutex);

    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        pthread_join(out->thread, NULL);
    }
    // TODO: detect pthread errors
}

void
collector_run(dns_collector_t *col)
{
    dns_ret_t r;

    for (char ** in = col->conf->inputs; *in; in++) {
        r = dns_collector_open_pcap_file(col, *in);
        assert(r == DNS_RET_OK);

        while(r = dns_collector_next_packet(col), r == DNS_RET_OK) { }
        if (r == DNS_RET_ERR)
            break;
    }
}


void
dns_collector_destroy(dns_collector_t *col)
{ 
    assert(col);

    if (col->tf_old)
        dns_timeframe_destroy(col->tf_old);

    if (col->tf_cur)
        dns_timeframe_destroy(col->tf_cur);

    if (col->pcap)
        pcap_close(col->pcap);
 
    pthread_mutex_destroy(&col->collector_mutex);
    pthread_cond_destroy(&col->output_cond);
    pthread_cond_destroy(&col->unblock_cond);

    free(col);
}


dns_ret_t
dns_collector_open_pcap_file(dns_collector_t *col, const char *pcap_fname)
{
    assert(col && pcap_fname);

    char pcap_errbuf[PCAP_ERRBUF_SIZE];

    if (col->pcap) {
        pcap_close(col->pcap);
        col->pcap = NULL;
    }

    col->pcap = pcap_open_offline(pcap_fname, pcap_errbuf);

    if (!col->pcap) {
        msg(L_ERROR, "libpcap error: %s", pcap_errbuf);
        return DNS_RET_ERR;
    }

    if (pcap_datalink(col->pcap) != DLT_RAW) {
        msg(L_ERROR, "pcap with link %s not supported (only DLT_RAW)", pcap_datalink_val_to_name(pcap_datalink(col->pcap)));
        pcap_close(col->pcap);
        col->pcap = NULL;
        return DNS_RET_ERR;
    }

    return DNS_RET_OK;
}


dns_ret_t
dns_collector_next_packet(dns_collector_t *col)
{
    assert(col);

    int r;
    struct pcap_pkthdr *pkt_header;
    const u_char *pkt_data;

    if (!col->pcap)
        return DNS_RET_EOF;

    r = pcap_next_ex(col->pcap, &pkt_header, &pkt_data);

    switch(r) {
        case -2: 
            return DNS_RET_EOF;

        case -1:
            msg(L_ERROR, "pcap: %s", pcap_geterr(col->pcap));
            return DNS_RET_ERR;

        case 0:
            return DNS_RET_TIMEOUT;

        case 1:
            col->stats.packets_captured++;
            dns_us_time_t now = dns_us_time_from_timeval(&(pkt_header->ts));

            if (!col->tf_cur) {
                dns_collector_rotate_frames(col, now);
            }

            // Possibly rotate several times to fill any gaps
            while (col->tf_cur->time_start + col->conf->timeframe_length < now) {
                dns_collector_rotate_frames(col, col->tf_cur->time_start + col->conf->timeframe_length);
            }
        

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

    // Matching request 
    dns_packet_t *req = NULL;

    if (DNS_PACKET_IS_RESPONSE(pkt)) {
        if (col->tf_old) {
            req = dns_timeframe_match_response(col->tf_old, pkt);
        }
        if (req == NULL) {
            req = dns_timeframe_match_response(col->tf_cur, pkt);
        }
    }

    if (req)
        return; // packet given to a matching request
        
    dns_timeframe_append_packet(col->tf_cur, pkt);
}


void
dns_collector_rotate_frames(dns_collector_t *col, dns_us_time_t time_now)
{
    pthread_mutex_lock(&col->collector_mutex);

    if (col->tf_old) {
        CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
            dns_output_push_frame(out, col->tf_old);
        }

        col->tf_old = NULL;
        pthread_cond_broadcast(&(col->output_cond));
    }

    pthread_mutex_unlock(&col->collector_mutex);

    if (time_now == DNS_NO_TIME) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        time_now = dns_us_time_from_timespec(&now);
    }

    if (col->tf_cur) {
        col->tf_cur -> time_end = time_now - 1; // prevent overlaps
        col->tf_old = col->tf_cur;
        col->tf_cur = NULL;
    }

    col->tf_cur = dns_timeframe_create(col->conf, time_now);
}

