#ifndef DNSCOL_COMMON_H
#define DNSCOL_COMMON_H


#define DNSCOL_MAX_FNAME_LEN 256

#define DNSCOL_ADDR_MAXLEN 16

/* Anonymous struct definitions. */
struct dns_stats;
typedef struct dns_stats dns_stats_t;

struct dns_collector_config;
typedef struct dns_collector_config dns_collector_config_t;

struct dns_collector;
typedef struct dns_collector dns_collector_t;

struct dns_timeframe;
typedef struct dns_timeframe dns_timeframe_t;

struct dns_packet;
typedef struct dns_packet dns_packet_t;

/* Enums */

/**
 * Packet direction
 *
 * * in - dst_addr is server addr in config
 * * out - src_addr is server addr in config
 * * unknown - none or both dst_addr, src_addr are server addr in config
 */

enum dns_packet_dir {
    dns_dir_unknown = 0,
    dns_dir_in = 1,
    dns_dir_out = 2,
};

/**
 * Packet drop/dump reasons
 *
 * * malformed - too short, bad headers, protocol, ...
 * * fragmented - when unimpl
 * * tcp - unimpl or weird stream
 * * direction - unknown addrs or wrong dir
 * * port - not a configured port packet
 * * bad_dns - bad dns header (odd type, weird nums, ...)
 * * frame_full - request/response table is full
 * * no_query - response without known request
 * * other - unknown reason
 */

enum dns_drop_reason {
    dns_drop_other = 0,
    dns_drop_malformed,
    dns_drop_fragmented,
    dns_drop_tcp,
    dns_drop_direction,
    dns_drop_port,
    dns_drop_bad_dns,
    dns_drop_frame_full,
    dns_drop_no_query,
    dns_drop_LAST // highest possible value (array sizes)
};

/**
 * dnscol return / error codes
 */

enum dns_ret {
    DNS_RET_OK = 0,
    DNS_RET_ERR = -1,
    DNS_RET_EOF = 1,
    DNS_RET_TIMEOUT = 2,
    DNS_RET_DROPPED = 3,
}

#endif /* DNSCOL_COMMON_H */
