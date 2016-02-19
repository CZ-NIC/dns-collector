
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

dns_packet_t*
dns_packet_create()
{
    return (dns_packet_t *)xmalloc_zero(sizeof(dns_packet_t));
}

void
dns_packet_destroy(dns_packet_t *pkt)
{
    if (pkt->dns_data)
        free(pkt->dns_data);
    if (pkt->dns_qname_string)
        free(pkt->dns_qname_string);
    if (pkt->response)
        dns_packet_destroy(pkt->response);
    free(pkt);
}

void
dns_drop_packet(dns_collector_t *col, dns_packet_t* pkt, enum dns_drop_reason reason)
{
    assert(col && pkt && pkt->pkt_data && reason < dns_drop_LAST);

    col->stats.packets_dropped++;
    col->stats.packets_dropped_reason[reason]++;

    CLIST_FOR_EACH(struct dns_output*, out, col->config->outputs) {
        if (out->drop_packet)
            out->drop_packet(out, pkt, reason);
    }
}

void
dns_packet_from_pcap(dns_collector_t *col, dns_packet_t* pkt, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    assert(col && pkt && pkt_header && pkt_data);

    pkt->ts = dns_us_time_from_timeval(&(pkt_header->ts));
    pkt->pkt_len = pkt_header->len;
    pkt->pkt_caplen = pkt_header->caplen;
    pkt->pkt_data = pkt_data;
}


dns_ret_t
dns_packet_parse_ip(dns_collector_t *col, dns_packet_t* pkt, uint32_t *header_offset)
{
    assert(col && pkt && pkt->pkt_data && header_offset && (pkt->pkt_caplen > (*header_offset)));

    const u_char *data = pkt->pkt_data + (*header_offset);

    if ((data[0] & 0xf0) == 0x40) {

        if (pkt->pkt_caplen < sizeof(struct ip) + (*header_offset)) {
            // DROP: no space for IPv4 header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        // ensure proper memory alignment
        struct ip hdr4;
        memcpy(&hdr4, data, sizeof(struct ip));

        pkt->ip_ver = 4;
        memcpy(pkt->src_addr, &(hdr4.ip_src), 4);
        memcpy(pkt->dst_addr, &(hdr4.ip_dst), 4);
        pkt->ip_proto = hdr4.ip_p;
        if (!DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)) {
            // DROP: unknown protocol (ICMP, ...)
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }

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

        (*header_offset) = hdr4.ip_hl * 4;
        return DNS_RET_OK;

    } else if ((data[0] & 0xf0) == 0x60) {

        if (pkt->pkt_caplen < sizeof(struct ip6_hdr) + (*header_offset)) {
            // DROP: no space for IPv6 header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        // ensure proper memory alignment
        struct ip6_hdr hdr6;
        memcpy(&hdr6, data, sizeof(struct ip6_hdr));

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

        (*header_offset) = sizeof(struct ip6_hdr);
        return DNS_RET_OK;

    } else {
        // DROP: neither IPv4 or IPv6
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

}

dns_ret_t
dns_packet_parse_proto(dns_collector_t *col, dns_packet_t* pkt, uint32_t *header_offset)
{
    assert(col && pkt && pkt->pkt_data && header_offset && (pkt->pkt_caplen > (*header_offset)));

    const u_char *data = pkt->pkt_data + (*header_offset);

    if (pkt->ip_proto == IPPROTO_UDP) {

        if (pkt->pkt_caplen < sizeof(struct udphdr) + (*header_offset)) {
            // DROP: no space for UDP header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        // ensure proper memory alignment
        struct udphdr udp_header;
        memcpy(&udp_header, data, sizeof(struct udphdr));

        pkt->src_port = ntohs(udp_header.source);
        pkt->dst_port = ntohs(udp_header.dest);
        if (ntohs(udp_header.len) != pkt->pkt_len - (*header_offset)) {
            // DROP: data length mismatch
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }
        // the following are positive by the above
        pkt->dns_len = pkt->pkt_len - (*header_offset) - sizeof(struct udphdr);
        pkt->dns_caplen = pkt->pkt_caplen - (*header_offset) - sizeof(struct udphdr);

        (*header_offset) += sizeof(struct udphdr);
        return DNS_RET_OK;
        
    } else {

        if (pkt->pkt_caplen < sizeof(struct tcphdr) + (*header_offset)) {
            // DROP: no space for TCP header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        // ensure proper memory alignment
        struct tcphdr tcp_header;
        memcpy(&tcp_header, data, sizeof(struct tcphdr));
        
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
            ((*header_offset) + tcp_header_len + sizeof(uint16_t) > pkt->pkt_caplen)) {
            // DROP: not enough data or bad header length
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        pkt->dns_len = pkt->pkt_len - (*header_offset) - tcp_header_len - sizeof(uint16_t);
        pkt->dns_caplen = pkt->pkt_caplen - (*header_offset) - tcp_header_len - sizeof(uint16_t);

        // ensure proper memory alignment
        uint16_t dns_len_net;
        memcpy(&dns_len_net, data + tcp_header_len, sizeof(uint16_t));

        if (ntohs(dns_len_net) != pkt->dns_len) {
            // DROP: not a single-packet TCP stream, or malformed
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }

        (*header_offset) += tcp_header_len + sizeof(uint16_t);
        return DNS_RET_OK;
    }
}

dns_ret_t
dns_packet_parse_dns(dns_collector_t *col, dns_packet_t* pkt, uint32_t *header_offset)
{
    assert(col && pkt && pkt->pkt_data && header_offset && (pkt->pkt_caplen > (*header_offset)));
    assert(pkt->dns_data == NULL);

    if ((*header_offset) + sizeof(dns_hdr_t) > pkt->pkt_caplen) {
        // DROP: no space for DNS header
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    pkt->dns_caplen = MIN(pkt->dns_caplen, MAX(col->config->capture_limit, sizeof(dns_hdr_t)));

    // ensure proper memory alignment
    pkt->dns_data = xmalloc(pkt->dns_caplen);
    memcpy(pkt->dns_data, pkt->pkt_data + (*header_offset), pkt->dns_caplen);
    (*header_offset) += sizeof(dns_hdr_t); // now points after DNS header

    if ((ntohs(pkt->dns_data->qs) != 1) ||
        (DNS_HDR_FLAGS_OPCODE(pkt->dns_data->flags) != 0 )) {
        // DROP: wrong # of DNS qnames
        free(pkt->dns_data);
        dns_drop_packet(col, pkt, dns_drop_bad_dns);
        return DNS_RET_DROPPED;
    }

    pkt->dns_qname_raw = pkt->dns_data->data;
    int32_t r = dns_query_check(pkt->dns_qname_raw, pkt->pkt_caplen - (*header_offset));
    if ((r < 0) || ((*header_offset) + r + 2 * sizeof(uint16_t) > pkt->pkt_caplen)) {
        // DROP: qname invalid, compressed or longer than captured data
        free(pkt->dns_data);
        dns_drop_packet(col, pkt, dns_drop_bad_dns);
        return DNS_RET_DROPPED;
    }
    pkt->dns_qname_raw_len = r;
    if (pkt->dns_qname_raw_len > DNS_PACKET_QNAME_MAX_LEN) {
        // DROP: qname too long
        free(pkt->dns_data);
        dns_drop_packet(col, pkt, dns_drop_bad_dns);
        return DNS_RET_DROPPED;
    }

    pkt->dns_qname_string = xmalloc(pkt->dns_qname_raw_len);
    dns_query_to_printable(pkt->dns_qname_raw, pkt->dns_qname_string);
    (*header_offset) += r; // now points to DNS query type

    // ensure proper memory alignment
    uint16_t tmp;
    memcpy(&tmp, pkt->pkt_data + (*header_offset), sizeof(uint16_t));
    pkt->dns_qtype = ntohs(tmp);
    (*header_offset) += sizeof(uint16_t); // now points to DNS query class
    memcpy(&tmp, pkt->pkt_data + (*header_offset), sizeof(uint16_t));
    pkt->dns_qclass = ntohs(tmp);
    (*header_offset) += sizeof(uint16_t); // now points to first RR

    return DNS_RET_OK;
}

dns_ret_t
dns_packet_parse(dns_collector_t *col, dns_packet_t* pkt)
{
    assert(col && pkt && pkt->pkt_data);

    if ((pkt->pkt_caplen < DNS_PACKET_MIN_LEN) || (pkt->pkt_caplen > pkt->pkt_len) || (pkt->pkt_len > DNS_PACKET_MAX_LEN)) {
        // DROP: basic size assumptions
        dns_drop_packet(col, pkt, dns_drop_malformed);
        return DNS_RET_DROPPED;
    }

    // header offset
    uint32_t header_len = 0;
    dns_ret_t r;

    if ((r = dns_packet_parse_ip(col, pkt, &header_len)) != DNS_RET_OK)
        return r;

    if ((r = dns_packet_parse_proto(col, pkt, &header_len)) != DNS_RET_OK)
        return r;

    if ((r = dns_packet_parse_dns(col, pkt, &header_len)) != DNS_RET_OK)
        return r;

    return DNS_RET_OK;
}



uint16_t
dns_packet_get_qclass(const dns_packet_t* pkt)
{
    assert(pkt && pkt->dns_data && pkt->dns_qname_raw);
    uint16_t tmp;
    // ensure proper alignment
    memcpy(&tmp, pkt->dns_qname_raw + pkt->dns_qname_raw_len + sizeof(uint16_t), sizeof(uint16_t));
    return ntohs(tmp);
}

uint16_t
dns_packet_get_qtype(const dns_packet_t* pkt)
{
    assert(pkt && pkt->dns_data && pkt->dns_qname_raw);
    uint16_t tmp;
    // ensure proper alignment
    memcpy(&tmp, pkt->dns_qname_raw + pkt->dns_qname_raw_len, sizeof(uint16_t));
    return ntohs(tmp);
}

int
dns_packets_match(const dns_packet_t* request, const dns_packet_t* response)
{
    assert(request && request->dns_data && response && response->dns_data);
    assert(DNS_PACKET_IS_REQUEST(request) &&
           DNS_PACKET_IS_RESPONSE(response));

    const int addr_len = (request->ip_ver == 4 ? 4 : 16);

    return (request->ip_ver == response->ip_ver &&
            request->ip_proto == response->ip_proto &&
            request->src_port == response->dst_port &&
            request->dst_port == response->src_port &&
            request->dns_data->id == response->dns_data->id && // network byte-order
            memcmp(request->src_addr, response->dst_addr, addr_len) == 0 &&
            memcmp(request->dst_addr, response->src_addr, addr_len) == 0 &&
            request->dns_qname_raw_len == response->dns_qname_raw_len &&
            memcmp(request->dns_qname_raw, response->dns_qname_raw, request->dns_qname_raw_len) == 0);
}

uint64_t
dns_packet_hash(const dns_packet_t* pkt, uint64_t param)
{
    assert(pkt && pkt->dns_data && param > 0x100);

    uint64_t hash = ((uint64_t)(pkt->ip_ver) << 0) +
                    ((uint64_t)(pkt->ip_proto) << 16) +
                    // Treat src and dst symmetrically
                    (((uint64_t)(pkt->dst_port + pkt->src_port)) << 32) +
                    ((uint64_t)(pkt->dns_data->id) << 48);
    hash = (hash % param) << 32;

    for (int i = 0; i < pkt->dns_qname_raw_len; i++)
        hash = hash + ((uint64_t)(pkt->dns_qname_raw[i]) << i);
    hash = (hash % param) << 32;

    for (int i = 0; i < DNS_ADDR_LEN(pkt->ip_ver); i++)
        // Treat src and dst symmetrically
        hash = hash + (((uint64_t)(pkt->src_addr[i]) + pkt->dst_addr[i]) << i);
    hash = hash % param;

    return hash;
}

uint16_t
dns_packet_get_output_flags(const dns_packet_t* pkt)
{
    uint16_t flags = 0;

    if (pkt->ip_ver == 6)
        flags |= DNS_PACKET_PRTOCOL_IPV6;
    if (pkt->ip_proto == IPPROTO_TCP)
        flags |= DNS_PACKET_PROTOCOL_TCP;
    if (DNS_PACKET_IS_REQUEST(pkt))
        flags |= DNS_PACKET_HAS_REQUEST;
    if (DNS_PACKET_IS_RESPONSE(pkt) || (pkt->response))
        flags |= DNS_PACKET_HAS_RESPONSE;

    return flags;
}
