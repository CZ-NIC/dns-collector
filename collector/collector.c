
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "collector.h"
#include "timeframe.h"
#include "packet.h"

dns_collector_t *
dns_collector_create(const dns_collector_config_t *conf)
{
    assert(conf);

    dns_collector_t *col = (dns_collector_t *)calloc(sizeof(dns_collector_t), 1);
    if (!col) return NULL;
  
    col->config = conf;

    col->pcap = pcap_open_dead(DLT_RAW, conf->capture_limit);
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
    
    // TODO: destroy frames
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

            dns_collector_process_packet(col, pkt_header, pkt_data);

            return DNS_RET_OK;
    }

    assert(0);
}

void
dns_collector_process_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    assert(col && pkt_header && pkt_data);

    dns_packet_t *pkt = (dns_packet_t *)malloc(sizeof(dns_packet_t));
    assert(pkt);

    dns_packet_from_pcap(col, pkt, pkt_header, pkt_data);
    dns_ret_t r = dns_parse_packet(col, pkt);
    if (r == DNS_RET_DROPPED) {
        free(pkt);
        return;
    }

    // TODO: gt a packet - YAY! do something next ...
    //fprintf(stdout, "#");
    free(pkt);
}


