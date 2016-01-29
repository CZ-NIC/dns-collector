#ifndef DNSCOL_DNS_H
#define DNSCOL_DNS_H

#include <netinet/in.h>

struct dns_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qs;
    uint16_t ans_rrs;
    uint16_t auth_rrs;
    uint16_t add_rrs;
    u_char data[];
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

/**
 * Check a dns-encoded query.
 *
 * Returns -1 on query longer that caplen, when a "compressed" label is found,
 * or when '\0' is part of the query string.
 *
 * Returns lenght including the final 0 otherwise. In this case, query is
 * a well-behaved zero-delimited string.
 */
int32_t
dns_query_check(u_char *query, uint32_t caplen);

/**
 * Convert a given QNAME to a printable o-terminated string with dots.
 *
 * Assumes the `qname` passes `dns_query_check()` and that `output` can hold
 * the entire raw qname.
 *
 * Replaces any characters not in [a-zA-Z0-9-] by '#', returns the number
 * of such replacements (0 if all ok).
 */
int
dns_query_to_printable(u_char *query, char *output);

#endif /* DNSCOL_DNS_H */


