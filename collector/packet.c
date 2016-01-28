
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <pcap/pcap.h>

#include "common.h"
#include "packet.h"
#include "collector.h"
#include "stats.h"
#include "timeframe.h"

void
dns_drop_packet(dns_collector_t *col, dns_packet_t* pkt, dns_drop_reason_t reason)
{
    assert(col && pkt && pkt->pkt_data && reason < dns_drop_LAST);

    col->stats.packets_dropped++;
    col->stats.packets_dropped_reason[reason]++;
    if (col->timeframes[0]) {
        col->timeframes[0]->stats.packets_dropped++;        
        col->timeframes[0]->stats.packets_dropped_reason[reason]++;        
    }

    if (col->config->dump_packet_reason[reason])
    {
        // TODO: check dump (soft/hard) quota?

        if (col->pcap_dump) {
            const struct pcap_pkthdr hdr = {pkt->ts, pkt->pkt_caplen, pkt->pkt_len};
            pcap_dump((u_char *)(col->pcap_dump), &hdr, pkt->pkt_data);
            col->stats.packets_dumped++;
            col->stats.packets_dumped_reason[reason]++;
            if (col->timeframes[0]) {
                col->timeframes[0]->stats.packets_dumped++;        
                col->timeframes[0]->stats.packets_dumped_reason[reason]++;        
            }
        }
    }
}

void
dns_packet_from_pcap(dns_collector_t *col, dns_packet_t* pkt, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    assert(col && pkt && pkt_header && pkt_data);

    pkt->ts.tv_sec = pkt_header->ts.tv_sec;
    pkt->ts.tv_usec = pkt_header->ts.tv_usec;
    pkt->pkt_len = pkt_header->len;
    pkt->pkt_caplen = pkt_header->caplen;
    pkt->pkt_data = pkt_data;
}

dns_ret_t
dns_parse_packet(dns_collector_t *col, dns_packet_t* pkt)
{
    assert(col && pkt && pkt->pkt_data);

    if ((pkt->pkt_caplen < DNS_PACKET_MIN_LEN) || (pkt->pkt_caplen > pkt->pkt_len) || (pkt->pkt_len > DNS_PACKET_MAX_LEN)) {
        // DROP: basic size assumptions
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    // IPv4/6 header length
    uint32_t header_len;

    if ((pkt->pkt_data[0] & 0xf0) == 0x40) {
        // ensure proper memory alignment; enough data by DNS_PACKET_MIN_LEN
        struct ip hdr4;
        memcpy(&hdr4, pkt->pkt_data, sizeof(struct ip));

        pkt->ip_ver = 4;
        memcpy(pkt->src_addr, &(hdr4.ip_src), 4);
        memcpy(pkt->dst_addr, &(hdr4.ip_dst), 4);
        pkt->ip_proto = hdr4.ip_p;
        if (!DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)) {
            // DROP: unknown protocol (ICMP, ...)
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }
        header_len = hdr4.ip_hl * 4;

        uint16_t offset = ntohs(hdr4.ip_off);
        if ((offset & IP_MF) || (offset & IP_OFFMASK)) {
            // DROP: packet fragmented
            dns_drop_packet(col, pkt, dns_drop_fragmented);
            return DNS_RET_DROPPED;
        }
        if (pkt->pkt_len != ntohs(hdr4.ip_len)) {
            // DROP: size mismatch
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

    } else if ((pkt->pkt_data[0] & 0xf0) == 0x60) {
        // ensure proper memory alignment; enough data by DNS_PACKET_MIN_LEN
        struct ip6_hdr hdr6;
        memcpy(&hdr6, pkt->pkt_data, sizeof(struct ip6_hdr));

        pkt->ip_ver = 6;
        memcpy(pkt->src_addr, &(hdr6.ip6_src), 16);
        memcpy(pkt->dst_addr, &(hdr6.ip6_dst), 16);
        pkt->ip_proto = hdr6.ip6_nxt;

        // TODO: traverse extension IPv6 headers (fragmentation, ...)
        if (!DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)) {
            // DROP: extra headers or fragmented
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }
        header_len = sizeof ( struct ip6_hdr );

    } else {
        // DROP: neither IPv4 or IPv6
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    if (header_len + (pkt->ip_proto == IPPROTO_UDP ? sizeof(struct udphdr) : sizeof(struct tcphdr)) > pkt->pkt_caplen) {
        // DROP: no data for UDP header or TCP header
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    // DNS data offset from the packet start
    uint32_t data_offset;
    if (pkt->ip_proto == IPPROTO_UDP) {
        // ensure proper memory alignment
        struct udphdr udp_header;
        memcpy(&udp_header, pkt->pkt_data + header_len, sizeof(struct udphdr));

        pkt->src_port = ntohs(udp_header.source);
        pkt->dst_port = ntohs(udp_header.dest);
        if (ntohs(udp_header.len) != pkt->pkt_len - header_len) {
            // DROP: data length mismatch
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }
        data_offset = header_len + sizeof(struct udphdr);
        // the following are positive by the above
        pkt->dns_len = pkt->pkt_len - header_len - sizeof(struct udphdr);
        pkt->dns_caplen = pkt->pkt_caplen - header_len - sizeof(struct udphdr);
        
    } else {
        // ensure proper memory alignment
        struct tcphdr tcp_header;
        memcpy(&tcp_header, pkt->pkt_data + header_len, sizeof(struct tcphdr));
        
        pkt->src_port = ntohs(tcp_header.th_sport);
        pkt->dst_port = ntohs(tcp_header.th_dport);
        uint32_t tcp_header_len = tcp_header.th_off * 4;

        // TODO: implement TCP streams
        // Below: only accepting packets with exactly one query (may be fooled! no way to check for seq==1)
        
        if ((tcp_header.th_flags & TH_FIN) || (tcp_header.th_flags & TH_SYN)) {
            // DROP: unlikely to be a single-packet TCP stream
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }

        if ((tcp_header_len < sizeof(struct tcphdr)) ||
            (header_len + tcp_header_len + sizeof(uint16_t) +
             DNS_DNS_HEADER_MIN_LEN > pkt->pkt_caplen)) {
            // DROP: not enough data
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        data_offset = header_len + tcp_header_len + sizeof(uint16_t);
        pkt->dns_len = pkt->pkt_len - header_len - tcp_header_len - sizeof(uint16_t);
        pkt->dns_caplen = pkt->pkt_caplen - header_len - tcp_header_len - sizeof(uint16_t);

        // ensure proper memory alignment
        uint16_t dns_len_net;
        memcpy(&dns_len_net, pkt->pkt_data + header_len + tcp_header_len, sizeof(uint16_t));
        if (ntohs(dns_len_net) != pkt->dns_len) {
            // DROP: not a single-packet TCP stream, or malformed
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }
    }

    pkt->dns_caplen = MIN(pkt->dns_caplen, col->config->capture_limit);
    pkt->dns_data = malloc(pkt->dns_caplen);
    if (!pkt->dns_data) 
        die("Out of memory"); 
    memcpy(pkt->dns_data, pkt->pkt_data, pkt->dns_caplen);

    // TODO: HERE: Preparse DNS ID and QNAME

    return DNS_RET_OK;
}

