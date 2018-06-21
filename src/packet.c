/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


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
#include "packet_hash.h"

struct dns_packet*
dns_packet_create(void *dns_data, size_t dns_data_size)
{
    struct dns_packet *pkt = xmalloc_zero(sizeof(struct dns_packet));
    pkt->dns_data = xmalloc_zero(dns_data_size);
    if (dns_data) {
        memcpy(pkt->dns_data, dns_data, dns_data_size);
    }
    pkt->dns_data_size = dns_data_size;
    pkt->knot_packet = knot_pkt_new(pkt->dns_data, pkt->dns_data_size, NULL);
    pkt->memory_size = sizeof(dns_packet_t) + dns_data_size + sizeof(knot_pkt_t);
    
    return pkt;
}


dns_ret_t
dns_packet_create_from_libtrace(libtrace_packet_t *tp, struct dns_packet **pktp)
{
    assert(tp && pktp);
    *pktp = NULL;

    uint8_t proto;
    uint32_t remaining;
    void *dns_data = NULL;

    void *transport_data = trace_get_transport(tp, &proto, &remaining);
    if (!transport_data) {
        // No transport layer found
        return DNS_RET_DROP_NETWORK;
    }
    struct libtrace_ip *ip_hdr = trace_get_ip(tp);
    struct libtrace_ip6 *ip6_hdr = trace_get_ip6(tp);
    //struct libtrace_icmp *icmp_hdr = trace_get_icmp(tp);
    //struct libtrace_icmp6 *icmp6_hdr = trace_get_icmp6(tp);
    struct libtrace_tcp *tcp_hdr = trace_get_tcp(tp);
    struct libtrace_udp *udp_hdr = trace_get_udp(tp);

    // IPv4/6 fragmentation
    uint16_t frag_offset;
    uint8_t frag_more;
    frag_offset = trace_get_fragment_offset(tp, &frag_more);
    if ((frag_offset > 0) || (frag_more)) {
        // fragmented packet
        // TODO: reassemble IP fragments (very infrequent)
        return DNS_RET_DROP_FRAGMENTED;
    }

    // Protocol header skip
    switch(proto) {
        case TRACE_IPPROTO_TCP:
            {/* to allow declaration in case */} 
            // Check TCP packet for type etc. Drop SYN/FIN/...
            if (tcp_hdr->syn || tcp_hdr->fin) {
                return DNS_RET_DROP_TRANSPORT;
            }
            
            // Below we assume that the TCP packet contains exactly one DNS
            // message, verifying this by checking the 16 bits of DNS data
            // lenght at the beginning of the packet
            // TODO: Change here when implementing TCP reconstruction
            // TODO: Silently ignore SYN/ACK packets for now
            if (remaining < sizeof(uint16_t)) {
                return DNS_RET_DROP_NETWORK;
            }
            dns_data = trace_get_payload_from_tcp(transport_data, &remaining);
            size_t message_size = ntohs(*((uint16_t *)dns_data));
            dns_data = ((uint16_t *)dns_data) + 1;
            remaining -= sizeof(uint16_t);

            // Check whether the indicated size matches the remaining packet size
            if (message_size + sizeof(uint16_t) != trace_get_payload_length(tp)) {
                return DNS_RET_DROP_TRANSPORT;
            }
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
            return DNS_RET_DROP_TRANSPORT;
    }

    // Packet struct allocation, DNS data copy and data lengths, create knot packet
    struct dns_packet *pkt = dns_packet_create(dns_data, remaining);

    pkt->dns_data_size_orig = trace_get_payload_length(tp);

    // Timestamp
    struct timeval tv = trace_get_timeval(tp);
    pkt->ts = dns_us_time_from_timeval(&tv);

    // Addresses and ports
    if (!( trace_get_source_address(tp, (struct sockaddr *)&(pkt->src_addr)) &&
           trace_get_destination_address(tp, (struct sockaddr *)&(pkt->dst_addr))
         )) {
        dns_packet_destroy(pkt);
        return DNS_RET_DROP_NETWORK;
    }

    // Protocol and other net stats
    pkt->net_protocol = proto;
    pkt->net_size = trace_get_wire_length(tp);
    pkt->net_ttl = 0;
    if (ip_hdr) pkt->net_ttl = ip_hdr->ip_ttl;
    if (ip6_hdr) pkt->net_ttl = ip6_hdr->hlim;
    pkt->net_udp_sum = udp_hdr ? ntohs(udp_hdr->check) : 0;

    // Parse and check QNAME count, validity and length
    int r = knot_pkt_parse_question(pkt->knot_packet);
    if (r != DNS_RET_OK)
    {
        if (r != KNOT_EOK)
            r = DNS_RET_DROP_MALF;
        dns_packet_destroy(pkt);
        return r;
    }

    // Fully parse and check requests
    if (DNS_PACKET_IS_REQUEST(pkt)) {
        r = knot_pkt_parse(pkt->knot_packet, 0);
        if (r != DNS_RET_OK)
        {
            if (r == KNOT_ENOMEM)
                die("Out of memory allocating packet structures.");
            if (r != KNOT_EOK)
                r = DNS_RET_DROP_MALF;
            dns_packet_destroy(pkt);
            return r;
        }
    }

    // DNS ID - aty this point the entire header is present
    pkt->dns_id = knot_wire_get_id(pkt->dns_data);

    *pktp = pkt;
    return DNS_RET_OK;
}

void
dns_packet_destroy(struct dns_packet *pkt)
{
    if (pkt->response)
        dns_packet_destroy(pkt->response);
    knot_pkt_free(&pkt->knot_packet);
    xfree(pkt->dns_data);
    xfree(pkt);
}

dns_hash_value_t
dns_packet_primary_hash(const dns_packet_t* pkt, dns_hash_value_t param)
{
    assert(pkt && param > 0x100);

    dns_hash_value_t hash = 0;
    hash += (dns_hash_value_t)(DNS_PACKET_AF(pkt)) << 0;
    hash += (dns_hash_value_t)(pkt->net_protocol) << 16;
    // Treat src and dst symmetrically
    hash += (dns_hash_value_t)(DNS_SOCKADDR_PORT(&pkt->dst_addr)) << 32;
    hash += (dns_hash_value_t)(DNS_SOCKADDR_PORT(&pkt->src_addr)) << 32;
    hash += (dns_hash_value_t)(pkt->dns_id) << 48;
    hash = hash % param;

    for (int i = 0; i < DNS_SOCKADDR_ADDRLEN(&pkt->src_addr) / 4; i++) {
        // Treat src and dst symmetrically
        hash += ((dns_hash_value_t)((uint32_t *)(DNS_SOCKADDR_ADDR(&pkt->src_addr)))[i]) << 32;
        hash += ((dns_hash_value_t)((uint32_t *)(DNS_SOCKADDR_ADDR(&pkt->dst_addr)))[i]) << 32;
        hash = hash % param;
    }

    return hash;
}


int
dns_packet_qname_match(struct dns_packet *request, struct dns_packet *response)
{
    assert(request && response);
    assert(DNS_PACKET_IS_REQUEST(request) &&
           DNS_PACKET_IS_RESPONSE(response));
    if (response->knot_packet->qname_size == 1)
        return 1;
    if (request->knot_packet->qname_size != response->knot_packet->qname_size)
        return 0;
    const knot_dname_t *req_q = knot_pkt_qname(request->knot_packet);
    const knot_dname_t *res_q = knot_pkt_qname(response->knot_packet);
    if (res_q == NULL && req_q == NULL)
        return 1;
    return (req_q && res_q && knot_dname_is_equal(res_q, req_q));
}

int
dns_packet_primary_match(const struct dns_packet* pkt1, const struct dns_packet* pkt2)
{
    assert(pkt1 && pkt2);

    const int addr_len = DNS_SOCKADDR_ADDRLEN(&pkt1->src_addr);

    // Compare AF, proto, DNS ID
    if (DNS_PACKET_AF(pkt1) != DNS_PACKET_AF(pkt2) ||
        pkt1->net_protocol != pkt2->net_protocol ||
        pkt1->dns_id != pkt2->dns_id) return 0;

    if ((!DNS_PACKET_IS_REQUEST(pkt1)) == (!DNS_PACKET_IS_REQUEST(pkt2))) {
        // Same direction, comapre src-src, dst-dst
        return (
            DNS_SOCKADDR_PORT(&pkt1->src_addr) == DNS_SOCKADDR_PORT(&pkt2->src_addr) &&
            DNS_SOCKADDR_PORT(&pkt1->dst_addr) == DNS_SOCKADDR_PORT(&pkt2->dst_addr) &&
            memcmp(DNS_SOCKADDR_ADDR(&pkt1->src_addr), DNS_SOCKADDR_ADDR(&pkt2->src_addr), addr_len) == 0 &&
            memcmp(DNS_SOCKADDR_ADDR(&pkt1->dst_addr), DNS_SOCKADDR_ADDR(&pkt2->dst_addr), addr_len) == 0);
    } else {
        // Different directions, compare src-dst and vice versa
        return (
            DNS_SOCKADDR_PORT(&pkt1->src_addr) == DNS_SOCKADDR_PORT(&pkt2->dst_addr) &&
            DNS_SOCKADDR_PORT(&pkt1->dst_addr) == DNS_SOCKADDR_PORT(&pkt2->src_addr) &&
            memcmp(DNS_SOCKADDR_ADDR(&pkt1->src_addr), DNS_SOCKADDR_ADDR(&pkt2->dst_addr), addr_len) == 0 &&
            memcmp(DNS_SOCKADDR_ADDR(&pkt1->dst_addr), DNS_SOCKADDR_ADDR(&pkt2->src_addr), addr_len) == 0);
    }
}

