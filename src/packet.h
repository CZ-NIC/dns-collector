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

#ifndef DNSCOL_PACKET_H
#define DNSCOL_PACKET_H

/**
 * \file packet.h
 * DNS packet structure, parsing and utilities.
 */

#include <stdint.h>
#include <libtrace.h>
#include <libknot/libknot.h>
#include <arpa/inet.h>
#include <stddef.h>

#include "common.h"
#include "packet_hash.h"

#define DNS_PACKET_FROM_SECNODE(cnodep) (SKIP_BACK(struct dns_packet, secnode, (cnodep)))

/**
 * Main structure storing the packet data and parsed values.
 */

struct dns_packet {
    /** Circular linked list node */
    cnode node;

    /** Secondary sircular linked list node (used for hash bucket chains)  */
    cnode secnode;

    /** Timestamp [us since Epoch] */
    dns_us_time_t ts;

    /** Source address family, address and port. Used as `struct sockaddr` for both IPv4 and IPv6. */
    struct sockaddr_in6 src_addr;

    /** Destination address family, address and port. Used as `struct sockaddr` for both IPv4 and IPv6. */
    struct sockaddr_in6 dst_addr;

    /** Transport layer protocol number */
    uint8_t net_protocol;

    /** Length of the packet (before snapping etc.) */
    size_t net_size;

    /** Original packet TTL */
    uint8_t net_ttl;

    /** Original packet checksum if UDP. */
    uint16_t net_udp_sum;

    /** DNS ID */
    uint16_t dns_id;

    /** When this is a request, a pointer to an matching response.
     * The response packet is then owned by this packet. */
    struct dns_packet *response;

    /** DNS data copy, owned by the packet. */
    uint8_t *dns_data;

    /** Actual length of dns_data. */
    size_t dns_data_size;

    /** Length of the DNS data befora any shortening. */
    size_t dns_data_size_orig;

    /** libknot packet structure for DNS parsing. Owned by the packet.
     * Should be parsed at least until the question (QNAME, type, class) after creation.
     * Requests are fully parsed (all RRs). */
    knot_pkt_t *knot_packet;

    /** Estimate of total packet memory size (for resource limiting) */
    size_t memory_size;
};


/** @name Getters for packet DNS properties */
/** @{ */

/** Is the packet DNS request? */
#define DNS_PACKET_IS_REQUEST(pkt) (! DNS_PACKET_IS_RESPONSE(pkt))

/** Is the packet DNS response? */
#define DNS_PACKET_IS_RESPONSE(pkt) (knot_wire_get_qr((pkt)->knot_packet->wire) != 0)

/** Return the request part of the query (or NULL if response-only) */
#define DNS_PACKET_REQUEST(pkt) (DNS_PACKET_IS_REQUEST(pkt) ? (pkt) : NULL)

/** Return the response part of the query (or NULL if request-only) */
#define DNS_PACKET_RESPONSE(pkt) (DNS_PACKET_IS_RESPONSE(pkt) ? (pkt) : (pkt)->response)

/** @} */

/** @name Getters for `struct sockaddr` properties */
/** @{ */

/** Address family as AF_INET or AF_INET6 */
#define DNS_SOCKADDR_AF(sa) ( ((struct sockaddr*)(sa))->sa_family )

#define DNS_SOCKADDR_PORT(sa) ( DNS_SOCKADDR_AF(sa) == AF_INET ? \
                                ntohs(((struct sockaddr_in*)(void*)(sa))->sin_port) : \
                                ntohs(((struct sockaddr_in6*)(void*)(sa))->sin6_port) )

#define DNS_SOCKADDR_ADDR(sa) ( DNS_SOCKADDR_AF(sa) == AF_INET ? \
                                (void *)(&((struct sockaddr_in*)(void*)(sa))->sin_addr) : \
                                (void *)(&((struct sockaddr_in6*)(void*)(sa))->sin6_addr) )

#define DNS_SOCKADDR_ADDRLEN(sa) ( DNS_SOCKADDR_AF(sa) == AF_INET ? \
                                   sizeof(((struct sockaddr_in*)(void*)(sa))->sin_addr) : \
                                   sizeof(((struct sockaddr_in6*)(void*)(sa))->sin6_addr) )

/** @} */

/** @name Getters for packet network properties */
/** @{ */

#define DNS_PACKET_CLIENT_SOCKADDR(pkt) ( DNS_PACKET_IS_REQUEST(pkt) ? \
                                          (struct sockaddr*)(&(pkt)->src_addr) : \
                                          (struct sockaddr*)(&(pkt)->dst_addr) )

#define DNS_PACKET_SERVER_SOCKADDR(pkt) ( DNS_PACKET_IS_REQUEST(pkt) ? \
                                          (struct sockaddr*)(&(pkt)->dst_addr) : \
                                          (struct sockaddr*)(&(pkt)->src_addr) )

#define DNS_PACKET_AF(pkt) (DNS_SOCKADDR_AF(&(pkt)->src_addr))

#define DNS_PACKET_ADDRLEN(pkt) (DNS_SOCKADDR_ADDRLEN(&(pkt)->src_addr))

#define DNS_PACKET_CLIENT_PORT(pkt) (DNS_SOCKADDR_PORT(DNS_PACKET_CLIENT_SOCKADDR(pkt)))

#define DNS_PACKET_SERVER_PORT(pkt) (DNS_SOCKADDR_PORT(DNS_PACKET_SERVER_SOCKADDR(pkt)))

#define DNS_PACKET_CLIENT_ADDR(pkt) (DNS_SOCKADDR_ADDR(DNS_PACKET_CLIENT_SOCKADDR(pkt)))

#define DNS_PACKET_SERVER_ADDR(pkt) (DNS_SOCKADDR_ADDR(DNS_PACKET_SERVER_SOCKADDR(pkt)))

/** @} */

/**
 * Allocate and initialise `struct dns_packet` from given data.
 * The data is copied (need not dtay valid afterwards).
 * Initializes knot_packet but no parsing is done.
 * When dns_data == NULL allocates new block.
 */
struct dns_packet*
dns_packet_create(void *dns_data, size_t dns_data_size);

/**
 * Create `dns_packet` from a given `libtrace_packet_t`.
 * Copies DNS data from the packet, so the libtrace_packet_t is free to be reused.
 * The new packet address is stored in pktp when successfull (DNS_RET_OK).
 */
dns_ret_t
dns_packet_create_from_libtrace(libtrace_packet_t *tp, struct dns_packet **pktp);

/**
 * Free a given packet and its owned data.
 */
void
dns_packet_destroy(struct dns_packet *pkt);

/**
 * Compute a packet hash function parameterized by `param`, caching the value.
 * `param` is used as a modulus - make sure it is large enough.
 * Uses IPver, TCP/UDP, both port numbers (summetrically), both IPs (summetrically), DNS ID.
 * Does not use QNAME.
 */
dns_hash_value_t
dns_packet_primary_hash(const dns_packet_t* pkt, dns_hash_value_t param);

/**
 * Compare two packets as request+response by teir QNAME.
 * Return true when they match, false otherwise.
 * Note that packets match if the QNAMES match or response QNAME is empty
 * (e.g. in NOTIMPL messages).
 */
int
dns_packet_qname_match(struct dns_packet *request, struct dns_packet *response);


/**
 * Compare two packets by their (IPver, TCP/UDP, both port numbers, both IPs and DNS ID).
 * Takes into account which of the packets are request and response (if same type,
 * compares src1 to src2, dest1 to dest2, if different type, src1 to dest2, dest1 to src2).
 * Return true when they match, false otherwise.
 */
int
dns_packet_primary_match(const struct dns_packet* pkt1, const struct dns_packet* pkt2);



/**
 * Packet flags field bits definition (collector query flags).
 * Represent packet IP version, transport, whether the pair has a request
 * and whether the pair has a response.
 */

/** If set, the IP version is IPv6, otherwise IPv4. */
#define DNS_PACKET_PRTOCOL_IPV6 0x01
/** If set, the query pair contains its request packet. */
#define DNS_PACKET_HAS_REQUEST 0x02
/** If set, the query pair contains its response packet. */
#define DNS_PACKET_HAS_RESPONSE 0x04

/** Transport is UDP. */
#define DNS_PACKET_PROTOCOL_UDP 0x00
/** Transport is TCP. */
#define DNS_PACKET_PROTOCOL_TCP 0x10
/** Transport is ICMP. */
#define DNS_PACKET_PROTOCOL_ICMP 0x20
/** Transport is ICMP6. */
#define DNS_PACKET_PROTOCOL_ICMPV6 0x30

/**
 * Return the combined flags `DNS_PACKET_*` for the packet.
 */
//uint16_t
//dns_packet_get_output_flags(const struct dns_packet* pkt);

/**
 * Drop and optionally dump a packet, depending on the reason and config.
 * Also records the packet in stats. May check the dump quota.
 * Does not destroy the packet.
 */
//void
//dns_drop_packet(dns_collector_t *col, struct dns_packet* pkt, enum dns_drop_reason reason);

/**
 * Parse initialised pkt and fill in all fields.
 *
 * Return DNS_RET_DROPPED on parsing failure and packet drop/dump.
 * In this case, dns_data == NULL.
 *
 * Returns DNS_RET_OK on success, in this case dns_data is allocated
 * and owned by the packet.
 */
//dns_ret_t
//dns_packet_parse(dns_collector_t *col, struct dns_packet* pkt);


#endif /* DNSCOL_PACKET_H */
