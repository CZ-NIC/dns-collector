
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

    // basic size assertions
    if ((pkt->pkt_caplen < DNS_PACKET_MIN_LEN) || (pkt->pkt_caplen > pkt->pkt_len) || (pkt->pkt_len > DNS_PACKET_MAX_LEN)) {
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }


    // IPv4/6 header
    struct ip *hdr4 = (struct ip *)pkt->pkt_data;
    struct ip6_hdr *hdr6 = (struct ip6_hdr *)pkt->pkt_data;
    uint32_t header_len;

    if (hdr4->ip_v == 4) {
        pkt->ip_ver = 4;

        memcpy(pkt->src_addr, &(hdr4->ip_src), 4);
        memcpy(pkt->dst_addr, &(hdr4->ip_dst), 4);
        pkt->ip_proto = hdr4->ip_p;
        if (!DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)) {
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }
        header_len = hdr4->ip_hl * 4;

        uint16_t offset = ntohs(hdr4->ip_off);
        if ((offset & IP_MF) || (offset & IP_OFFMASK)) {
            // Packet fragmented (first and more, or has offset)
            dns_drop_packet(col, pkt, dns_drop_fragmented);
            return DNS_RET_DROPPED;
        }
        if (pkt->pkt_len != ntohs(hdr4->ip_len)) {
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

    } else if ((hdr6->ip6_vfc & 0xf0) == 0x60) {
        pkt->ip_ver = 6;

        memcpy(pkt->src_addr, &(hdr6->ip6_src), 16);
        memcpy(pkt->dst_addr, &(hdr6->ip6_dst), 16);
        pkt->ip_proto = hdr6->ip6_nxt;
        // TODO: traverse extension IPv6 headers
        if (!DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)) {
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }
        header_len = sizeof ( struct ip6_hdr );

    } else {
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    // Must be assured by above
    assert(DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)); 

    // another size check - space for TCP/UDP and DNS header
    if (header_len + (pkt->ip_proto == IPPROTO_UDP ? 8 : 20) + DNS_DNS_HEADER_MIN_LEN > pkt->pkt_caplen) {
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    const u_char *proto_data = pkt->pkt_data + header_len;
    if (pkt->ip_proto == IPPROTO_UDP) {
        struct udphdr *udp_header = (struct udphdr *)proto_data;

        pkt->src_port = ntohs(udp_header->source);
        pkt->dst_port = ntohs(udp_header->dest);
        if (ntohs(udp_header->len) != pkt->pkt_len - header_len) {
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }
        // assured to be valid (and dns_caplen positive by the size check)
        pkt->dns_data = proto_data + 8;
        pkt->dns_len = pkt->pkt_len - header_len - 8;
        pkt->dns_caplen = pkt->pkt_caplen - header_len - 8;
        
    } else {
        struct tcphdr *tcp_header = (struct tcphdr *)proto_data;
        
        pkt->src_port = ntohs(tcp_header->th_sport);
        pkt->dst_port = ntohs(tcp_header->th_dport);
        uint32_t tcp_header_len = tcp_header->th_off * 4;

        // TODO: implement TCP streams
        // Below: only accept packets with exactly one query (may be fooled! no way to check for seq==1)
        
        if ((tcp_header->th_flags & TH_FIN) || (tcp_header->th_flags & TH_SYN)) {
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }

        if ((tcp_header_len < 20) || (header_len + tcp_header_len + 2 + DNS_DNS_HEADER_MIN_LEN > pkt->pkt_caplen)) {
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        pkt->dns_data = proto_data + tcp_header_len + 2;
        uint16_t query_len = ntohs(((uint16_t *)pkt->dns_data)[-1]);
        pkt->dns_len = pkt->pkt_len - header_len - tcp_header_len - 2;
        pkt->dns_caplen = pkt->pkt_caplen - header_len - tcp_header_len - 2;

        if (query_len != pkt->dns_len) {
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }
        // We have a packet with length exactly matching DNS query length
    }
    // TODO: HERE: Preparse DNS

    return DNS_RET_OK;
}

