#ifndef DNSCOL_COMMON_H
#define DNSCOL_COMMON_H

/**
 * \file common.h
 * Collector common definitions
 */

// TODO: is msg() threadsafe?

#define DNS_WITH_CSV
//#define DNS_WITH_PROTO

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <libknot/libknot.h>

#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ucw/lib.h>
#include <ucw/log.h>
#include <ucw/conf.h>
#include <ucw/gary.h>
#include <ucw/opt.h>
#pragma GCC diagnostic pop

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

struct dns_output;

/* Enums */

/**
 * Dnscol return and error codes.
 * Extends enum knot_error;
 */

enum dns_ret {
    DNS_RET_OK = 0,

    DNS_RET_ERR = KNOT_ERROR_MIN - 1000,
    DNS_RET_EOF,
    DNS_RET_TIMEOUT,
    DNS_RET_DROP_NETWORK, // Network layers parsing error
    DNS_RET_DROP_FRAGMENTED, // TCP fragments unsupported
    DNS_RET_DROP_TRANSPORT, // Unsupported transport

    /* Sentinel */
    DNS_RET_LAST,

    /* Some errors mapped to KNOT errors */
    DNS_RET_DROP_MALF = KNOT_EMALF, // Malformed or short DNS data
};

typedef enum dns_ret dns_ret_t;

/**
 * Common output field list.
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
 * Packet parsing error or drop reason.
 */
enum dns_parse_error {
    DNS_PE_OK = 0,
    DNS_PE_NETWORK,
    DNS_PE_FRAGMENTED,
    DNS_PE_TRANSPORT,
    DNS_PE_DNS,
    DNS_PE_LIMIT,
    DNS_PE_LAST // Sentinel
};
typedef enum dns_parse_error dns_parse_error_t;

/**
 * Packet drop/dump reasons.
 */

enum dns_drop_reason {
    dns_drop_other = 0,  ///< Unknown reason.
    dns_drop_malformed,  ///< Too short, bad headers, protocol, ...
    dns_drop_fragmented, ///< IP defrag not implemented.
    dns_drop_protocol,   ///< Unimplemented (now TCP)
    dns_drop_bad_dns,    ///< Bad dns header or query count != 1
    dns_drop_limit,      ///< Rate/resource-limiting.
    dns_drop_LAST // Sentinel
};

extern const char *dns_drop_reason_names[];


/**
 * Internal debugging: print the current trace to stderr.
 */
void dns_ptrace(void);


/**
 * Time type for timestamps and time differences.
 * Absolute time in micro-seconds since the Epoch.
 * Holds up to + and - 292 000 years.
 */
typedef int64_t dns_us_time_t;

/** Value representing "no time given". */
#define DNS_NO_TIME (INT64_MIN)

/**
 * Convert a given `struct timeval` to `dns_us_time_t`.
 */
dns_us_time_t
dns_us_time_from_timeval(const struct timeval *t);

/**
 * Convert a given `struct timespec` to `dns_us_time_t`.
 */
dns_us_time_t
dns_us_time_from_timespec(const struct timespec *t);

/**
 * Convert `dns_us_time_t` to seconds (as `double`).
 */
double
dns_us_time_to_fsec(dns_us_time_t t);

/**
 * Convert seconds (as `double`) to `dns_us_time_t`.
 */
dns_us_time_t
dns_fsec_to_us_time(double s);

/**
 * Format a string with `strftime()` for UTC from given `dns_us_time_t`.
 * Return value, `s`, `max` and `format` same as `strftime()`.
 */
size_t
dns_us_time_strftime(char *s, size_t max, const char *format, dns_us_time_t time);

/**
 * Logging type flag indicating possibly very frequent message 
 * (IO errors ...) to be rate-limited.
 */
#define DNS_MSG_SPAM (LS_SET_TYPE(log_find_type("spam")))

#endif /* DNSCOL_COMMON_H */
