#ifndef DNSCOL_COLLECTOR_CONFIG_H
#define DNSCOL_COLLECTOR_CONFIG_H

#include "common.h"
#include "stats.h"

/** Configuration of a DNS collector. */
struct dns_config {

    dns_us_time_t timeframe_length;
    double timeframe_length_sec;
    int32_t capture_limit;
    struct clist outputs_pcap; // for config only, later empty
    struct clist outputs_proto; // for config only, later empty
    struct clist outputs_csv; // for config only, later empty
    struct clist outputs; // clst of collected struct dns_output from outputs_csv, outputs_proto, outputs_pcap
    char **inputs; // not owned by config, NULL-terminated
    int32_t hash_order;

    /* Dump dropped packets by reason */
    int dump_packet_reason[dns_drop_LAST];
};

extern struct cf_section dns_config_section;

#endif /* DNSCOL_COLLECTOR_CONFIG_H */
