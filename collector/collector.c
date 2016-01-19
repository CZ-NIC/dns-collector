
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "collector.h"

dns_collector_t *
dns_collector_create(const dns_collector_config_t *conf)
{
    dns_collector_t *col = (dns_collector_t *)calloc(sizeof(dns_collector_t) + conf->active_frames * sizeof(dns_timeframe_t *), 1);
    if (!col) return NULL;
  
    col->config = conf;

    return col;
}


void
dns_collector_destroy(dns_collector_t *col)
{ 
    dns_collector_dump_close(col);

    if (col->pcap) {
        pcap_close(col->pcap);
        col->pcap = NULL;
    }

    // TODO: destroy frames
    
    free(col);
}


int
dns_collector_open_pcap(dns_collector_t *col, const char *pcap_fname)
{
    char pcap_errbuf[PCAP_ERRBUF_SIZE];

    assert(col);

    if (col->pcap) {
        pcap_close(col->pcap);
        col->pcap = NULL;
    }

    col->pcap = pcap_open_offline(pcap_fname, pcap_errbuf);

    if (!col->pcap) {
        fprintf(stderr, "libpcap error: %s\n", pcap_errbuf);
        return -1;
    }

    if (pcap_datalink(col->pcap) != DLT_RAW) {
        fprintf(stderr, "error: pcap with link %s not supported\n", pcap_datalink_val_to_name(pcap_datalink(col->pcap)));
        pcap_close(col->pcap);
        col->pcap = NULL;
        return -1;
    }

    return 0;
}


int
dns_collector_dump_open(dns_collector_t *col, const char *dump_fname)
{
    assert(col);

    dns_collector_dump_close(col);
    col->pcap_dump = pcap_dump_open(col->pcap, dump_fname);

    if (!col->pcap_dump) {
        fprintf(stderr, "libpcap error: %s\n", pcap_geterr(col->pcap));
        return -1;
    }

    return 0;
}

void
dns_collector_dump_close(dns_collector_t *col)
{
    assert(col);
    
    if (col->pcap_dump) {
        pcap_dump_close(col->pcap_dump);
        col->pcap_dump = NULL;
    }
}

int
dns_collector_dump_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    assert(col);
    if (!col->pcap_dump) {
        return -1;
    }

    pcap_dump((u_char *)(col->pcap_dump), pkt_header, pkt_data);
    col->stats.packets_dumped++;
    // TODO: frame stats

    return 0;
}

int
dns_collector_next_packet(dns_collector_t *col)
{
    int r;
    struct pcap_pkthdr *pkt_header;
    const u_char *pkt_data;

    assert(col);
    if (!col->pcap) {
        return -1;
    }

    r = pcap_next_ex(col->pcap, &pkt_header, &pkt_data);
    if (r <= 0) {
        return r;
    }
    assert(r == 1); 

    col->stats.packets_captured++;
    // TODO: frame stats

    r = dns_collector_process_packet(col, pkt_header, pkt_data);

    if (r == 0)
        return 1;
    return -1;
}

int
dns_collector_process_packet(dns_collector_t *col, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    // TODO: add some processing :)
    dns_collector_dump_packet(col, pkt_header, pkt_data);
    
    return 0;
}

void
dns_collector_write_stats(dns_collector_t *col, FILE *f)
{
    assert(col);
    fprintf(f, "packets_captured: %ld\n", col->stats.packets_captured);
    fprintf(f, "packets_exceptional: %ld\n", col->stats.packets_exceptional);
    fprintf(f, "packets_dumped: %ld\n", col->stats.packets_dumped);
}
