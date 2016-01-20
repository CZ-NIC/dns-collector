#ifndef DNSCOL_PACKET_H
#define DNSCOL_PACKET_H

#include <stdint.h>
#include <pcap/pcap.h>

#include "common.h"

struct dns_packet {
    // Pcap data - borrowed from PCAP, only valid during first processing
    struct pcap_pkthdr *pkt_header;
    const u_char *pkt_data;

    // IP packet info
    uint8_t ip_ver; // 4 or 6
    dns_packet_dir_t dir; // IN means dst_addr is known
    // for IPv4, only _addr[0..3] matter
    uint8_t src_addr[DNSCOL_ADDR_MAXLEN]; 
    uint8_t dst_addr[DNSCOL_ADDR_MAXLEN];

    // TCP/UDP packet info
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t ip_proto; // TCP or UDP

    // DNS packet TODO: specify ownership
    const char *dns_data;
    uint32_t dns_len;
};


/**
 * Drop and optionally dump a packet, depending on the reason and config.
 * Also records the packet in stats. May checks the dump quota.
 * Does not deallocate any memory.
 */
void
dns_drop_packet(dns_collector_t *col, dns_packet_t* pkt, dns_drop_reason_t reason);

/**
 * Initialize pkt with data from pkt_header,pkt_data and begin parsing.
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 * Returns DNS_RET_OK when the packet is pre-parsed up to DNS header (dns_data is set).
 * Does no de/allocation.
 */
dns_ret_t
dns_parse_packet(dns_collector_t *col, dns_packet_t* pkt, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);

#endif /* DNSCOL_PACKET_H */
