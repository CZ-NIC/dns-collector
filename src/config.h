#ifndef DNSCOL_COLLECTOR_CONFIG_H
#define DNSCOL_COLLECTOR_CONFIG_H

/**
 * \file config.h
 * DNS collector configuration.
 */

#include "common.h"
#include "stats.h"
#include "input.h"
#include "output.h"

/** Configuration of a DNS collector. Filled in mostly by the `libucw` config system.
 * See `dnscol.conf` for detaled description. */
struct dns_config {

    dns_us_time_t timeframe_length;
    double timeframe_length_sec;

    int32_t capture_limit;

    int max_queue_len;
    
    int wait_for_outputs; ///< If set, the queued frames are never dropped. The collector waits instead.

    struct clist outputs_csv; ///< For config only, later empty.
    struct clist outputs_proto; ///< For config only, later empty.
    struct clist outputs_cbor; ///< For config only, later empty.
    struct clist outputs; ///< clist of collected struct dns_output from outputs_*.

//    char **inputs; ///< Not owned by config, NULL-terminated array.

    int32_t hash_order;

    struct dns_input input;

    /** Whether to also dump dropped packets by drop reason. */
    int dump_packet_reason[dns_drop_LAST];
};

extern struct cf_section dns_config_section;

#endif /* DNSCOL_COLLECTOR_CONFIG_H */
