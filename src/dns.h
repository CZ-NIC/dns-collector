#ifndef DNSCOL_DNS_H
#define DNSCOL_DNS_H

/**
 * \file dns.h
 * Simple DNS packet header and QNAME parsing.
 */

#include <netinet/in.h>

/**
 * DNS packet header (as on the wire).
 */
struct dns_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qs;
    uint16_t ans_rrs;
    uint16_t auth_rrs;
    uint16_t add_rrs;
};


#define DNS_HDR_FLAGS_QR(flags)     ((ntohs(flags) & 0x8000) >> 15)
#define DNS_HDR_FLAGS_OPCODE(flags) ((ntohs(flags) & 0x7800) >> 11)
#define DNS_HDR_FLAGS_AA(flags)     ((ntohs(flags) & 0x0400) >> 10)
#define DNS_HDR_FLAGS_TC(flags)     ((ntohs(flags) & 0x0200) >> 9)
#define DNS_HDR_FLAGS_RD(flags)     ((ntohs(flags) & 0x0100) >> 8)

#define DNS_HDR_FLAGS_RA(flags)     ((ntohs(flags) & 0x0080) >> 7)
#define DNS_HDR_FLAGS_Z(flags)      ((ntohs(flags) & 0x0040) >> 6)
#define DNS_HDR_FLAGS_AD(flags)     ((ntohs(flags) & 0x0020) >> 5)
#define DNS_HDR_FLAGS_CD(flags)     ((ntohs(flags) & 0x0010) >> 4)
#define DNS_HDR_FLAGS_RCODE(flags)  ((ntohs(flags) & 0x000f) >> 0)

/** Maximum length of a QNAME by the RFC */

#define DNS_PACKET_QNAME_MAX_LEN 255

/**
 * Check the length and validity of a dns-encoded query.
 *
 * \return QNAME lenght including the final '\0' otherwise. In this case, query is
 * a well-behaved zero-delimited string. On error (QNAME exceeding `maxlen`,
 * when a "compressed" label is found, or when the query string contains '\0')
 * returns -1.
 */
int32_t
dns_query_check(u_char *query, uint32_t maxlen);

/**
 * Convert a given QNAME to a printable 0-terminated string with dots as separators.
 *
 * Assumes the `query` passes `dns_query_check()` and that `output` can hold
 * the entire raw qname. Replaces any characters not in [a-zA-Z0-9-] by '#'.
 * \return The number of such '#' replacements (0 if all OK).
 */
int
dns_query_to_printable(u_char *query, char *output);

#endif /* DNSCOL_DNS_H */


