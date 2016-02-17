#ifndef DNSCOL_WRITEPROTO_H
#define DNSCOL_WRITEPROTO_H

#include "common.h"
#include "dnsquery.pb-c.h"
#include "packet.h"
#include "collector_config.h"

/** Buffer large enough to hold serialised DnsQuery protobuf.
 * All fixed-size attributes should take <=96 bytes even at 8b/attr,
 * IPs and 2xQNAME should fit 4*4+2*16+2*256, 656 bytes total. */
#define DNS_MAX_PROTO_LEN 1024

/**
 * Reset and fill in a given `proto` with combined `request` and `response`.
 *
 * At least one of `request` and `response` need to be given, one may be NULL.
 * Assumes `dns_packets_match(request, response)` is true.
 * Uses `conf` to select included fields.
 */
void
dns_fill_proto(const struct dns_collector_config *conf, const dns_packet_t* request, const dns_packet_t* response, DnsQuery *proto);

#endif /* DNSCOL_WRITEPROTO_H */
