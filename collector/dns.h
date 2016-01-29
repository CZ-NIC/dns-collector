#ifndef DNSCOL_DNS_H
#define DNSCOL_DNS_H

struct dns_hdr {
    uint16_t id;
    uint8_t f_qr :1;
    uint8_t f_opcode :4;
    uint8_t f_aa :1;
    uint8_t f_tc :1;
    uint8_t f_rd :1;
    uint8_t f_ra :1;
    uint8_t f_z :1;
    uint8_t f_ad :1;
    uint8_t f_cd :1;
    uint8_t f_rcode :4;
    uint16_t qs;
    uint16_t ans_rrs;
    uint16_t auth_rrs;
    uint16_t add_rrs;
    u_char data[];
};

/**
 * Return the value of all flags of a given dns header in host byte-order.
 */
uint16_t
dns_hdr_flags(const struct dns_hdr *hdr);

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


