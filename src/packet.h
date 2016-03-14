#ifndef DNSCOL_PACKET_H
#define DNSCOL_PACKET_H

/**
 * \file packet.h
 * DNS packet structure, parsing and utilities.
 */

#include <stdint.h>
#include <libtrace.h>

#include "common.h"
#include "dns.h"

/**
 * Main structure storing the packet data and parsed values.
 */

struct dns_packet {
    // Phys. layer
    
    /** Timestamp [us since Epoch] */
    dns_us_time_t ts;

    /** Source address family, address and port. Used as `struct sockaddr` for both IPv4 and IPv6. */
    struct sockaddr_in6 src_addr;

    /** Destination address family, address and port. Used as `struct sockaddr` for both IPv4 and IPv6. */
    struct sockaddr_in6 dst_addr;

    /** Transport layer protocol number */
    uint8_t protocol;

    /** When this is a request, a pointer to an optional matching response.
     * Owned by this packet. */
    struct dns_packet *response;

    /** When a packet is in timeframe hash, next packet with the same hash.
     * Not owned by this packet. */
    struct dns_packet *next_in_hash;

    /** When a packet is in timeframe packet sequence, next packet in the sequence.
     * Not owned by this packet. */
    struct dns_packet *next_in_timeframe;

    /** The length of original DNS packet (starting at the DNS header). */
    uint32_t dns_orig_len;

    /** Pointer to raw QNAME within `dns_data`. */
    u_char *dns_qname_raw;

    /** Length of qname_raw (incl. the final '\0' */
    uint32_t dns_qname_raw_len;

    /** Query type (host byte-order). */
    uint16_t dns_qtype;

    /** Query class (host byte-order). */
    uint16_t dns_qclass;

    /** Captured DNS data length (starting at the DNS header).
     * Parsing the packet only succeeds if the captured data contains the header, QNAME, QCLASS and QTYPE. */
    uint32_t dns_data_len;

    /** DNS packet data starting with `struct dns_hdr` allocated directly after `struct dns_packet`.
     * This data is in network byte-order!
     *
     * This entry MUST be last in `struct dns_packet`. */
    struct dns_hdr dns_data[];
};


/** @name Getters for packet DNS properties */
/** @{ */

/** Is the packet DNS request? */
#define DNS_PACKET_IS_REQUEST(pkt) (DNS_HDR_FLAGS_QR((pkt)->dns_data->flags) == 0)

/** Is the packet DNS response? */
#define DNS_PACKET_IS_RESPONSE(pkt) (DNS_HDR_FLAGS_QR((pkt)->dns_data->flags) == 1)

/** Return the request part of the query (or NULL id response-only) */
#define DNS_PACKET_REQUEST(pkt) (DNS_PACKET_IS_REQUEST(pkt) ? (pkt) : NULL)

/** Return the response part of the query (or NULL id request-only) */
#define DNS_PACKET_RESPONSE(pkt) (DNS_PACKET_IS_RESPONSE(pkt) ? (pkt) : (pkt)->response)

/** @} */

/** @name Getters for `struct sockaddr` properties */
/** @{ */

/** Address family as AF_INET or AF_INET6 */
#define DNS_SOCKADDR_AF(sa) ( ((struct sockaddr*)(sa))->sa_family )

#define DNS_SOCKADDR_PORT(sa) ( DNS_SOCKADDR_AF(sa) == AF_INET ? \
                                ((struct sockaddr_in*)(void*)(sa))->sin_port : \
                                ((struct sockaddr_in6*)(void*)(sa))->sin6_port )

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

/*
inline uint16_t
dns_sockaddr_port(void *sa)
{
    if (DNS_SOCKADDR_AF(sa) == AF_INET)
        return ((struct sockaddr_in*)(void*)sa)->sin_port;
    if (DNS_SOCKADDR_AF(sa) == AF_INET6)
        return ((struct sockaddr_in6*)(void*)sa)->sin6_port;
    assert(!"Unsupported sockaddr AF");
}
#define DNS_SOCKADDR_PORT(sa) dns_sockaddr_port(sa)

inline u_char *
dns_sockaddr_addr(struct sockaddr *sa)
{
    if (DNS_SOCKADDR_AF(sa) == AF_INET)
        return (u_char *)&(((struct sockaddr_in*)(void*)sa)->sin_addr);
    if (DNS_SOCKADDR_AF(sa) == AF_INET6)
        return (u_char *)&(((struct sockaddr_in6*)(void*)sa)->sin6_addr);
    assert(!"Unsupported sockaddr AF");
}
#define DNS_SOCKADDR_ADDR(sa) dns_sockaddr_addr(sa)

inline size_t
dns_sockaddr_addrlen(struct sockaddr *sa)
{
    if (DNS_SOCKADDR_AF(sa) == AF_INET)
        return 4;
    if (DNS_SOCKADDR_AF(sa) == AF_INET6)
        return 16;
    assert(!"Unsupported sockaddr AF");
}
#define DNS_SOCKADDR_ADDRLEN(sa) dns_sockaddr_addrlen(sa)

inline struct sockaddr *
dns_packet_client_sockaddr(struct dns_packet *pkt)
{
    if (DNS_PACKET_IS_REQUEST(pkt))
        return (struct sockaddr*)(&(pkt)->src_addr);
    else
        return (struct sockaddr*)(&(pkt)->dst_addr);
}
#define DNS_PACKET_CLIENT_SOCKADDR(pkt) dns_packet_server_sockaddr(pkt)

inline struct sockaddr *
dns_packet_server_sockaddr(struct dns_packet *pkt)
{
    if (DNS_PACKET_IS_RESPONSE(pkt))
        return (struct sockaddr*)(&(pkt)->src_addr);
    else
        return (struct sockaddr*)(&(pkt)->dst_addr);
}
#define DNS_PACKET_SERVER_SOCKADDR(pkt) dns_packet_server_sockaddr(pkt)
*/


/**
 * Allocate and initialise `struct dns_packet` with `dns_data_len` extra bytes for dns header and data.
 * Sets `dns_data_len`.
 */
dns_packet_t*
dns_packet_create(size_t extra_size);

/**
 * Create `dns_packet` from a given `libtrace_packet_t`.
 * Copies DNS data from the packet and parses the DNS packet data.
 * Returns NULL on any error and in that case `err` is set.
 */
dns_packet_t*
dns_packet_create_from_libtrace(dns_collector_t *col, libtrace_packet_t *tp, dns_parse_error_t *err);

/**
 * Free a given packet and its owned data.
 */
void
dns_packet_destroy(dns_packet_t *pkt);

/**
 * Drop and optionally dump a packet, depending on the reason and config.
 * Also records the packet in stats. May check the dump quota.
 * Does not destroy the packet.
 */
void
dns_drop_packet(dns_collector_t *col, dns_packet_t* pkt, enum dns_drop_reason reason);

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
 * Compare two packets as request+response.
 * Return true when they match, false otherwise.
 * Assumes `dns_packet_parse_dns()` was run successfully on both.
 * Uses IPver, TCP/UDP, both port numbers, both IPs, DNS ID and QNAME.
 */
int
dns_packets_match(const dns_packet_t* request, const dns_packet_t* response);

/**
 * Compute a packet hash function parameterized by `param`.
 * `param` is used as a modulo - make sure it is large enough.
 * Assumes `dns_packet_parse_dns()` was run successfully.
 * Uses IPver, TCP/UDP, both port numbers, both IPs, DNS ID and QNAME.
 */
uint64_t
dns_packet_hash(const dns_packet_t* pkt, uint64_t param);

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
uint16_t
dns_packet_get_output_flags(const dns_packet_t* pkt);



#endif /* DNSCOL_PACKET_H */
