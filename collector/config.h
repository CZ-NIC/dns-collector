#ifndef DNSCOL_COLLECTOR_CONFIG_H
#define DNSCOL_COLLECTOR_CONFIG_H

#include <ucw/lib.h>
#include <ucw/conf.h>

#include "common.h"
#include "stats.h"

/** Configuration of a DNS collector. */
struct dns_config {

    dns_us_time_t timeframe_length;
    double timeframe_length_sec;
    int32_t capture_limit;
    struct clist outputs; // clst of struct dns_output
    char **inputs; // not owned by config, NULL-terminated
    int32_t hash_order;

    /* Dump dropped packets by reason */
    int dump_packet_reason[dns_drop_LAST];
};

extern struct cf_section dns_config_section;

#endif /* DNSCOL_COLLECTOR_CONFIG_H */
