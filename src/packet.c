
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <libtrace.h>

#include "common.h"
#include "packet.h"
#include "collector.h"
#include "stats.h"
#include "timeframe.h"

dns_packet_t*
dns_packet_create(size_t dns_data_len)
{
    dns_packet_t *pkt = xmalloc_zero(sizeof(dns_packet_t) + dns_data_len);
    pkt->dns_data_len = dns_data_len;
    return pkt;
}

/**
 * Create `dns_packet` from a given `libtrace_packet_t`.
 * Copies DNS data from the packet.
 */
dns_packet_t*
dns_packet_create_from_libtrace(dns_collector_t *col UNUSED, libtrace_packet_t *tp, dns_parse_error_t *err)
{
    uint8_t proto;
    uint32_t remaining;
    void *transport_data = trace_get_transport(tp, &proto, &remaining);
    void *dns_data = NULL;
    dns_parse_error_t dummy;
    if (!err)
        err = &dummy;

    if (!transport_data) {
        // No transport layer found
        *err = DNS_PE_NETWORK;
        return NULL;
    }

    // IPv4/6 fragmentation
    uint16_t frag_offset;
    uint8_t frag_more;
    frag_offset = trace_get_fragment_offset(tp, &frag_more);
    if ((frag_offset > 0) || (frag_more)) {
        // fragmented packet
        // TODO: reassemble?
        *err = DNS_PE_FRAGMENTED;
        return NULL;
    }

    // Protocol header skip
    switch(proto) {
        case TRACE_IPPROTO_TCP:
            *err = DNS_PE_TRANSPORT;
            return NULL;
            // TODO: Check TCP packet for type etc. Drop SYN/ACK/...
            //dns_data = trace_get_payload_from_tcp(transport_data, &remaining);
            break;
        case TRACE_IPPROTO_UDP:
            dns_data = trace_get_payload_from_udp(transport_data, &remaining);
            break;
        case TRACE_IPPROTO_ICMP:
            dns_data = trace_get_payload_from_icmp(transport_data, &remaining);
            break;
        case TRACE_IPPROTO_ICMPV6:
            dns_data = trace_get_payload_from_icmp6(transport_data, &remaining);
            break;
        default:
            *err = DNS_PE_TRANSPORT;
            return NULL;
    }

    if ((!dns_data) || (remaining < sizeof(struct dns_hdr))) {
        *err = DNS_PE_NETWORK;
        return NULL;
    }

    int is_request UNUSED = (DNS_HDR_FLAGS_QR(((struct dns_hdr*)dns_data)->flags) == 0);
    // TODO: Limit copied data length for responses (probably just to hdr+qname+qtype+qclass)
    size_t dns_copy = remaining;
    if (dns_copy < sizeof(struct dns_hdr)) {
        *err = DNS_PE_NETWORK;
        return NULL;
    }

    // Packet allocation, DNS data copy and data lengths
    struct dns_packet *pkt = dns_packet_create(dns_copy);

    memcpy(pkt->dns_data, dns_data, dns_copy);
    // pkt->dns_data_len already set
    pkt->dns_orig_len = trace_get_payload_length(tp);
    if (pkt->dns_orig_len == 0) {
        *err = DNS_PE_NETWORK;
        dns_packet_destroy(pkt);
        return NULL;
    }

    // Timestamp
    struct timeval tv = trace_get_timeval(tp);
    pkt->ts = dns_us_time_from_timeval(&tv);

    // Addresses and ports
    if (!( trace_get_source_address(tp, (struct sockaddr *)&(pkt->src_addr)) &&
           trace_get_destination_address(tp, (struct sockaddr *)&(pkt->dst_addr))
         )) {
        *err = DNS_PE_NETWORK;
        dns_packet_destroy(pkt);
        return NULL;
    }

    // Protocol
    pkt->protocol = proto;

    // QNAME validity and length
    if (ntohs(pkt->dns_data->qs) != 1) {
        *err = DNS_PE_DNS;
        dns_packet_destroy(pkt);
        return NULL;
    }
    pkt->dns_qname_raw = ((u_char *)(pkt->dns_data)) + sizeof(struct dns_hdr);
    pkt->dns_qname_raw_len = dns_query_check(pkt->dns_qname_raw, pkt->dns_data_len - sizeof(struct dns_hdr));
    if (pkt->dns_qname_raw_len <= 0) {
        // qname invalid
        *err = DNS_PE_DNS;
        dns_packet_destroy(pkt);
        return NULL;
    }
    if (sizeof(struct dns_hdr) + pkt->dns_qname_raw_len + 2 * sizeof(uint16_t) > pkt->dns_data_len) {
        // captured data too short
        *err = DNS_PE_NETWORK;
        dns_packet_destroy(pkt);
        return NULL;
    }

    // QTYPE, QCLASS
    // ensure proper memory alignment by memcpy
    uint16_t tmp[2];
    memcpy(tmp, pkt->dns_qname_raw + pkt->dns_qname_raw_len, sizeof(tmp));
    pkt->dns_qtype = ntohs(tmp[0]);
    pkt->dns_qclass = ntohs(tmp[1]);

    *err = DNS_PE_OK;
    return pkt;
}

void
dns_packet_destroy(dns_packet_t *pkt)
{
    if (pkt->response)
        dns_packet_destroy(pkt->response);
    xfree(pkt);
}

int
dns_packets_match(const dns_packet_t* request, const dns_packet_t* response)
{
    assert(request && response);
    assert(DNS_PACKET_IS_REQUEST(request) &&
           DNS_PACKET_IS_RESPONSE(response));

    const int addr_len = DNS_SOCKADDR_ADDRLEN(&request->src_addr);

    return (DNS_PACKET_AF(request) == DNS_PACKET_AF(response) &&
            request->protocol == response->protocol &&
            DNS_SOCKADDR_PORT(&request->src_addr) == DNS_SOCKADDR_PORT(&response->dst_addr) &&
            DNS_SOCKADDR_PORT(&request->dst_addr) == DNS_SOCKADDR_PORT(&response->src_addr) &&
            request->dns_data->id == response->dns_data->id && // network byte-order ids
            memcmp(DNS_SOCKADDR_ADDR(&request->src_addr), DNS_SOCKADDR_ADDR(&response->dst_addr), addr_len) == 0 &&
            memcmp(DNS_SOCKADDR_ADDR(&request->dst_addr), DNS_SOCKADDR_ADDR(&response->src_addr), addr_len) == 0 &&
            request->dns_qname_raw_len == response->dns_qname_raw_len &&
            memcmp(request->dns_qname_raw, response->dns_qname_raw, request->dns_qname_raw_len) == 0);
}

uint64_t
dns_packet_hash(const dns_packet_t* pkt, uint64_t param)
{
    assert(pkt && param > 0x100);

    uint64_t hash = 0;
    hash += (uint64_t)(DNS_PACKET_AF(pkt)) << 0;
    hash += (uint64_t)(pkt->protocol) << 16;
    // Treat src and dst symmetrically
    hash += (uint64_t)(DNS_SOCKADDR_PORT(&pkt->dst_addr)) << 32;
    hash += (uint64_t)(DNS_SOCKADDR_PORT(&pkt->src_addr)) << 32;
    hash += (uint64_t)(pkt->dns_data->id) << 48;

    for (int i = 0; i < pkt->dns_qname_raw_len; i++) {
        if (i % 32 == 0)
            hash = hash % param;
        hash += ((uint64_t)(pkt->dns_qname_raw[i]) << (i % 32)) << 32;
    }
    hash = hash % param;

    for (int i = 0; i < DNS_SOCKADDR_ADDRLEN(&pkt->src_addr) / 4; i++) {
        // Treat src and dst symmetrically
        hash += (uint64_t)((uint32_t *)(DNS_SOCKADDR_ADDR(&pkt->src_addr)))[i] << 32;
        hash += (uint64_t)((uint32_t *)(DNS_SOCKADDR_ADDR(&pkt->dst_addr)))[i] << 32;
        hash = hash % param;
    }

    return hash;
}

uint16_t
dns_packet_get_output_flags(const dns_packet_t* pkt)
{
    uint16_t flags = 0;

    if (DNS_PACKET_AF(pkt) == AF_INET6)
        flags |= DNS_PACKET_PRTOCOL_IPV6;

    if (pkt->protocol == TRACE_IPPROTO_TCP)
        flags |= DNS_PACKET_PROTOCOL_TCP;

    if (pkt->protocol == TRACE_IPPROTO_ICMP)
        flags |= DNS_PACKET_PROTOCOL_ICMP;

    if (pkt->protocol == TRACE_IPPROTO_ICMPV6)
        flags |= DNS_PACKET_PROTOCOL_ICMPV6;

    if (DNS_PACKET_IS_REQUEST(pkt))
        flags |= DNS_PACKET_HAS_REQUEST;

    if (DNS_PACKET_IS_RESPONSE(pkt) || (pkt->response))
        flags |= DNS_PACKET_HAS_RESPONSE;

    return flags;
}

void
dns_drop_packet(dns_collector_t *col, dns_packet_t* pkt, enum dns_drop_reason reason)
{
    assert(col && pkt && reason < dns_drop_LAST);

    col->stats.packets_dropped++;
    col->stats.packets_dropped_reason[reason]++;

    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
// TODO: refactor paket dropping/dumping
//        if (out->drop_packet)
//            out->drop_packet(out, pkt, reason);
    }
}

/* Obsolete parsing - might reuse part for libtrace 
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
    // assert proper memory alignment
    assert((size_t)(data) % 2 == 0);

    if ((data[0] & 0xf0) == 0x40) {

        if (pkt->pkt_caplen < sizeof(struct ip) + (*header_offset)) {
            // DROP: no space for IPv4 header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        struct ip *hdr4 = (struct ip *) data;

        pkt->ip_ver = 4;
        memcpy(pkt->src_addr, &(hdr4->ip_src), 4);
        memcpy(pkt->dst_addr, &(hdr4->ip_dst), 4);
        pkt->ip_proto = hdr4->ip_p;
        if (!DNS_ACCEPTED_PROTOCOL(pkt->ip_proto)) {
            // DROP: unknown protocol (ICMP, ...)
            dns_drop_packet(col, pkt, dns_drop_protocol);
            return DNS_RET_DROPPED;
        }

        uint16_t offset = ntohs(hdr4->ip_off);
        if ((offset & IP_MF) || (offset & IP_OFFMASK)) {
            // DROP: packet fragmented
            dns_drop_packet(col, pkt, dns_drop_fragmented);
            return DNS_RET_DROPPED;
        }
        if (pkt->pkt_len != ntohs(hdr4->ip_len)) {
            // DROP: size mismatch
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        (*header_offset) = hdr4->ip_hl * 4;
        return DNS_RET_OK;

    } else if ((data[0] & 0xf0) == 0x60) {

        if (pkt->pkt_caplen < sizeof(struct ip6_hdr) + (*header_offset)) {
            // DROP: no space for IPv6 header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        struct ip6_hdr *hdr6 = (struct ip6_hdr *) data;

        pkt->ip_ver = 6;
        memcpy(pkt->src_addr, &(hdr6->ip6_src), 16);
        memcpy(pkt->dst_addr, &(hdr6->ip6_dst), 16);
        pkt->ip_proto = hdr6->ip6_nxt;

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
    // assert proper memory alignment
    assert((size_t)(data) % 2 == 0);

    if (pkt->ip_proto == IPPROTO_UDP) {

        if (pkt->pkt_caplen < sizeof(struct udphdr) + (*header_offset)) {
            // DROP: no space for UDP header
            dns_drop_packet(col, pkt, dns_drop_malformed);
            return DNS_RET_DROPPED;
        }

        // ensure proper memory alignment
        struct udphdr *udp_header = (struct udphdr *) data;

        pkt->src_port = ntohs(udp_header->source);
        pkt->dst_port = ntohs(udp_header->dest);
        if (ntohs(udp_header->len) != pkt->pkt_len - (*header_offset)) {
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
        struct tcphdr *tcp_header = (struct tcphdr *) data;
        
        pkt->src_port = ntohs(tcp_header->th_sport);
        pkt->dst_port = ntohs(tcp_header->th_dport);
        uint32_t tcp_header_len = tcp_header->th_off * 4;

        // TODO: implement TCP streams
        // Below: only accepting packets with exactly one query (may be fooled! no way to check for seq==1)
        
        if ((tcp_header->th_flags & TH_FIN) || (tcp_header->th_flags & TH_SYN)) {
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
dns_packet_parse(dns_collector_t *col, dns_packet_t* pkt)
{
    assert(col && pkt && pkt->pkt_data);

    if ((pkt->pkt_caplen < DNS_PACKET_MIN_LEN) || (pkt->pkt_caplen > pkt->pkt_len) || (pkt->pkt_caplen > col->conf->capture_limit)) {
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

*/


