#ifndef DNSCOL_COMMON_H
#define DNSCOL_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <ucw/lib.h>
#include <ucw/log.h>

#define DNSCOL_MAX_FNAME_LEN 256

#define DNSCOL_ADDR_MAXLEN 16

/* Anonymous struct definitions. */
struct dns_stats;
typedef struct dns_stats dns_stats_t;

struct dns_collector;
typedef struct dns_collector dns_collector_t;

struct dns_timeframe;
typedef struct dns_timeframe dns_timeframe_t;

struct dns_packet;
typedef struct dns_packet dns_packet_t;

struct dns_hdr;
typedef struct dns_hdr dns_hdr_t;

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
typedef enum dns_packet_dir dns_packet_dir_t;

/**
 * dnscol return / error codes
 */

enum dns_ret {
    DNS_RET_OK = 0,
    DNS_RET_ERR = -1,
    DNS_RET_EOF = 1,
    DNS_RET_TIMEOUT = 2,
    DNS_RET_DROPPED = 3,
};
typedef enum dns_ret dns_ret_t;

/**
 * Common optional output fields
 */

enum dns_output_field {
    dns_of_flags = 0,

    dns_of_client_addr,
    dns_of_client_port,
    dns_of_server_addr,
    dns_of_server_port,

    dns_of_id,
    dns_of_qname,
    dns_of_qtype,
    dns_of_qclass,

    dns_of_request_time_us,
    dns_of_request_flags,
    dns_of_request_ans_rrs,
    dns_of_request_auth_rrs,
    dns_of_request_add_rrs,
    dns_of_request_length,

    dns_of_response_time_us,
    dns_of_response_flags,
    dns_of_response_ans_rrs,
    dns_of_response_auth_rrs,
    dns_of_response_add_rrs,
    dns_of_response_length,

    dns_of_LAST, // Sentinel
};

extern const char *dns_output_field_names[];

/**
 * Packet drop/dump reasons
 *
 * * malformed - too short, bad headers, protocol, ...
 * * fragmented - when unimpl
 * * protocol - unimplemented (now tcp) or other proto (ICMP)
 * * direction - unknown addrs or wrong dir
 * * port - not a configured port
 * * bad_dns - bad dns header (odd type, weird nums, ...)
 * * frame_full - request/response table is full
 * * no_query - response without known request
 * * other - unknown reason
 */

enum dns_drop_reason {
    dns_drop_other = 0,
    dns_drop_malformed,
    dns_drop_fragmented,
    dns_drop_protocol,
    dns_drop_direction,
    dns_drop_port,
    dns_drop_bad_dns,
    dns_drop_frame_full,
    dns_drop_no_request,
    dns_drop_LAST // Sentinel
};

extern const char *dns_drop_reason_names[];

/**
 * Time for timestamps and time differences.
 * Absolute time in micro-seconds since the Epoch.
 * Holds up to +- 292 000 years.
 */
typedef int64_t dns_us_time_t;

/** Value representing "no time given". */
#define DNS_NO_TIME (INT64_MIN)

dns_us_time_t
dns_us_time_from_timeval(const struct timeval *t);

dns_us_time_t
dns_us_time_from_timespec(const struct timespec *t);

double
dns_us_time_to_fsec(dns_us_time_t t);

dns_us_time_t
dns_fsec_to_us_time(double s);

/**
 * Format a string with `strftime()` for UTC from given `dns_us_time_t`.
 * Return value, `s`, `max` and `format` same as `strftime()`.
 */
size_t
dns_us_time_strftime(char *s, size_t max, const char *format, dns_us_time_t time);

/**
 * Logging flag indicating a possibly very frequent message 
 * (IO errors ...) to be rate-limited.
 */
#define DNS_MSG_SPAM (LS_SET_TYPE(log_find_type("spam")))

#endif /* DNSCOL_COMMON_H */
