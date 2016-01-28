#ifndef DNSCOL_DNS_H
#define DNSCOL_DNS_H

struct dnshdr {
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


#endif /* DNSCOL_DNS_H */


