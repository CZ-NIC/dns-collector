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
 * Convert a given QNAME to a printable 0-terminated string with dots as separators and check its validity.
 * Special in-label characters '.' and '\0' are converted to '#'.
 *
 * @param qname  Raw qname string.
 * @param qname_maxlen  Maximum length of `qname` data.
 * @param output  Destination string. When `NULL`, just verify the qname.
 * @param output_len  Destination string maximum length including the fonal '\0'. Ignored when `output==NULL`.
 * 
 * \return QNAME raw lenght including the final '\0'. Returns `-1` when `qname` is compressed,
 * exceeds `qname_maxlen` or `DNS_PACKET_QNAME_MAX_LEN` or output exceeds `output_len`.
 */
int32_t
dns_qname_printable(u_char *qname, uint32_t qname_maxlen, char *output, size_t output_len);

#endif /* DNSCOL_DNS_H */


