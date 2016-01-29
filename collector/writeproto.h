#ifndef DNSCOL_WRITEPROTO_H
#define DNSCOL_WRITEPROTO_H

#include "common.h"
#include "dnsquery.pb-c.h"
#include "packet.h"

/**
 * Reset and fill in a given `proto` with combined `request` and `response`.
 *
 * At least one of `request` and `response` need to be given, one may be NULL.
 * Assumes `dns_packets_match(request, response)` is true.
 * Uses `conf` to select included fields.
 */
void
dns_fill_proto(const dns_collector_config_t *conf, const dns_packet_t* request, const dns_packet_t* response, DnsQuery *proto);

#endif /* DNSCOL_WRITEPROTO_H */
