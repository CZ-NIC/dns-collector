#ifndef DNSCOL_PACKET_H
#define DNSCOL_PACKET_H

#include <stdint.h>
#include <pcap/pcap.h>

#include "common.h"
#include "dns.h"

/** Smallest possible length of DNS query/response (only header) */
#define DNS_DNS_HEADER_MIN_LEN (12)

/** Packet min length (just smallest headers) */
#define DNS_PACKET_MIN_LEN (20 + 8 + DNS_DNS_HEADER_MIN_LEN)

/** Packet max length (hard limit) */
#define DNS_PACKET_MAX_LEN (32000)

/** TCP ot UDP protocol? */
#define DNS_ACCEPTED_PROTOCOL(p) (((p) == IPPROTO_UDP) || ((p) == IPPROTO_TCP))


struct dns_packet {
    /** Timestamp [us] */
    struct timeval ts;
    /** Total (claimed) length of packet, may be longer than pkt_caplen */
    uint32_t pkt_len;
    /** Captured data length, length of pkt_data */
    uint32_t pkt_caplen;
    /** Packet data inc. IP header, borrowed from PCAP,
     * only valid during first processing, no alignment assumptions */
    const u_char *pkt_data;

    // IP packet info
    uint8_t ip_ver; // 4 or 6
    dns_packet_dir_t dir; // direction based on known address
    // for IPv4, only _addr[0..3] matter
    uint8_t src_addr[DNSCOL_ADDR_MAXLEN]; 
    uint8_t dst_addr[DNSCOL_ADDR_MAXLEN];

    // TCP/UDP packet info
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t ip_proto; // TCP or UDP

    /** DNS packet data, aligned, owned by the packet if not NULL */
    dns_hdr_t *dns_data;
    /** Total length of original DNS packet */
    uint32_t dns_len;
    /** Captured data, sizeof(dns_header_t) <= dns_caplen <= dns_len */
    uint32_t dns_caplen;
    /** Pointer to QNAME */
    u_char *dns_qname;
    /** Length of QNAME (incl. the final '\0' */
    uint32_t dns_qname_len;
    /** Query type */
    uint16_t dns_qtype;
    /** Query class */
    uint16_t dns_qclass;
};


/**
 * Drop and optionally dump a packet, depending on the reason and config.
 * Also records the packet in stats. May checks the dump quota.
 * Does not deallocate any memory.
 */
void
dns_drop_packet(dns_collector_t *col, dns_packet_t* pkt, dns_drop_reason_t reason);

/**
 * Initialize given pkt with data from pcap pkt_header,pkt_data.
 * No allocation.
 */
void
dns_packet_from_pcap(dns_collector_t *col, dns_packet_t* pkt, struct pcap_pkthdr *pkt_header, const u_char *pkt_data);

/**
 * Parse initialised pkt up to the dns header.
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 * Returns DNS_RET_OK when the packet is pre-parsed up to DNS header (dns_data is set).
 * No de/allocation.
 */
dns_ret_t
dns_packet_parse(dns_collector_t *col, dns_packet_t* pkt);

#endif /* DNSCOL_PACKET_H */
