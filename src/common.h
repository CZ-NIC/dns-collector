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
    dns_field_timestamp = 0,
    dns_field_delay_us, 
    dns_field_req_dns_len,
    dns_field_resp_dns_len, 
    dns_field_req_net_len,
    dns_field_resp_net_len,
    dns_field_client_addr,
    dns_field_client_port, 
    dns_field_server_addr,
    dns_field_server_port, 
    dns_field_net_proto,
    dns_field_net_ipv, 
    dns_field_net_ttl, 
    dns_field_req_udp_sum,
    dns_field_id,
    dns_field_qtype, 
    dns_field_qclass, 
    dns_field_opcode,
    dns_field_rcode,
    dns_field_flags, 
    dns_field_resp_aa = dns_field_flags, 
    dns_field_resp_tc = dns_field_flags, 
    dns_field_req_rd = dns_field_flags,
    dns_field_resp_ra = dns_field_flags,
    dns_field_req_z = dns_field_flags,
    dns_field_resp_ad = dns_field_flags,
    dns_field_req_cd = dns_field_flags,
    dns_field_qname,
    dns_field_rr_counts, 
    dns_field_resp_ancount = dns_field_rr_counts, 
    dns_field_resp_arcount = dns_field_rr_counts, 
    dns_field_resp_nscount = dns_field_rr_counts,
    dns_field_edns,
    dns_field_req_edns_ver = dns_field_edns, 
    dns_field_req_edns_udp = dns_field_edns,
    dns_field_req_edns_do = dns_field_edns, 
    dns_field_resp_edns_rcode = dns_field_edns, 
    dns_field_req_edns_ping = dns_field_edns,
    dns_field_req_edns_dau = dns_field_edns,
    dns_field_req_edns_dhu = dns_field_edns,
    dns_field_req_edns_n3u = dns_field_edns,
    dns_field_resp_edns_nsid = dns_field_edns, 
    dns_field_edns_client_subnet = dns_field_edns,
    dns_field_edns_other = dns_field_edns,

    dns_of_LAST, // Sentinel
};

extern const char *dns_output_field_flag_names[];

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
 * Global input stop flag.
 * After it is set, the input is stopped and the program
 * waits for pipeline to finish.
 */
extern int dns_global_stop;

/**
 * Internal debugging: print the current trace to stderr.
 */
void dns_ptrace(void);

/**
 * Extract an IP address string from a sockaddr.
 * Works for IPv4 and IPv6, returns `s` on success, NULL on error (but the message is still set)
 */
char *
dns_sockaddr_to_str(const struct sockaddr *sa, char *s, size_t maxlen);

/**
 * Write datasize bytes of data to str escaping special characters.
 * Writes no more than strsize bytes including the final '\0'.
 * Escapes '\', '\0', '\n' and the separator character.
 * Returns the number of characters written to str excluding the final '\0'.
 */
int
dns_snescape(char *str, size_t strsize, int separator, const uint8_t *data, size_t datasize);

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
 * Return current real time as dns_us_time_t
 */
dns_us_time_t
dns_current_us_time();

/**
 * Check whether a next rotation with given period should occur.
 * If now==DNS_NO_TIME, use current time. 
 */
int
dns_next_rotation(int period_sec, dns_us_time_t last_rotation, dns_us_time_t now);


/**
 * Logging type flag indicating possibly very frequent message 
 * (IO errors ...) to be rate-limited.
 */
extern int dns_log_spam_type;
#define DNS_MSG_SPAM (dns_log_spam_type)

#endif /* DNSCOL_COMMON_H */
