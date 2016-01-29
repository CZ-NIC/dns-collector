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

    /** DNS packet data, aligned, owned by the packet if not NULL.
     * Warning: this data is in network byte-order! */
    dns_hdr_t *dns_data;
    /** Total length of original DNS packet */
    uint32_t dns_len;
    /** Captured data, sizeof(dns_header_t) <= dns_caplen <= dns_len */
    uint32_t dns_caplen;
    /** Pointer to QNAME */
    u_char *dns_qname;
    /** Length of QNAME (incl. the final '\0' */
    uint32_t dns_qname_len;
    /** Query type, host byte-order */
    uint16_t dns_qtype;
    /** Query class host byte-order */
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
 * Parse initialised pkt and fill in all fields.
 *
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 * In this case, dns_data == NULL.
 *
 * Returns DNS_RET_OK on success, in this case dns_data is allocated
 * and owned by the packet.
 */
dns_ret_t
dns_packet_parse(dns_collector_t *col, dns_packet_t* pkt);

/**
 * Parse IPv4/6 header at offset *header_offset.
 * Detects IP version from the data.
 *
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 *
 * Returns DNS_RET_OK on success, *header_offset is the offset of
 * the TCP/UDP header.
 */
dns_ret_t
dns_packet_parse_ip(dns_collector_t *col, dns_packet_t* pkt, uint32_t *header_offset);

/**
 * Parse TCP/UDP header at offset *header_offset.
 * Needs pkt->ip_proto to be set.
 *
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 *
 * Returns DNS_RET_OK on success, *header_offset is the offset of
 * the DNS header.
 */
dns_ret_t
dns_packet_parse_proto(dns_collector_t *col, dns_packet_t* pkt, uint32_t *header_offset);

/**
 * Parse DNS header at offset *header_offset.
 * Drops some weird packet and only accepts packets with exactly one query.
 *
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 *
 * Returns DNS_RET_OK on success, *header_offset is the offset of
 * the first RR (if any) just after the QUERY, dns_data is allocated
 * and owned by the packet.
 */
dns_ret_t
dns_packet_parse_dns(dns_collector_t *col, dns_packet_t* pkt, uint32_t *header_offset);

/**
 * Return query qclass in host byte-order.
 * Assumes `dns_packet_parse_dns()` was run successfully.
 */
uint16_t
dns_packet_get_qclass(const dns_packet_t* pkt);

/**
 * Return query qtype in host byte-order.
 * Assumes `dns_packet_parse_dns()` was run successfully.
 */
uint16_t
dns_packet_get_qtype(const dns_packet_t* pkt);

/**
 * Returns packet timestamp in usec since Epoch.
 */
uint32_t
dns_packet_get_time_us(const dns_packet_t* pkt);

/**
 * Compare two packets as request+response.
 * Return true when they match, false otherwise.
 * Assumes `dns_packet_parse_dns()` was run successfully on both.
 */
int
dns_packets_match(dns_packet_t* request, dns_packet_t* response);


#endif /* DNSCOL_PACKET_H */
