#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <execinfo.h>
#include <arpa/inet.h>

#include "common.h"

int dns_global_stop = 0;

int dns_log_spam_type = 0;

#define MAX_TRACE_SIZE 42
void dns_ptrace(void)
{
    void *array[MAX_TRACE_SIZE];
    size_t size = backtrace(array, MAX_TRACE_SIZE);
    backtrace_symbols_fd(array, size, 2);
}

char *
dns_sockaddr_to_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
    case AF_INET:
	inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
	return s;

    case AF_INET6:
	inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
	return s;

    default:
	strncpy(s, "unknown_AF", maxlen);
	return NULL;
    }
}

dns_us_time_t
dns_us_time_from_timeval(const struct timeval *t)
{
    return t->tv_usec + (1000000 * t->tv_sec);
}


dns_us_time_t
dns_us_time_from_timespec(const struct timespec *t)
{
    return ((dns_us_time_t)(t->tv_nsec) / 1000) + (1000000LL * (dns_us_time_t)(t->tv_sec));
}


double
dns_us_time_to_fsec(dns_us_time_t t)
{
    assert(t != DNS_NO_TIME);
    return ((double)t) / 1000000.0;
}


dns_us_time_t
dns_fsec_to_us_time(double s)
{
    return (dns_us_time_t)(s * 1000000.0);
}


size_t
dns_us_time_strftime(char *s, size_t max, const char *format, dns_us_time_t time)
{
    assert(time != DNS_NO_TIME);
    struct tm tmp_tm;
    time_t tsec = time / 1000000;
    struct tm *res = gmtime_r(&tsec, &tmp_tm);
    assert(res);
    return strftime(s, max, format, &tmp_tm);
}

dns_us_time_t
dns_current_us_time()
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return dns_us_time_from_timespec(&now);
}

int
dns_next_rotation(int period_sec, dns_us_time_t last_rotation, dns_us_time_t now)
{
    if (now == DNS_NO_TIME)
        now = dns_current_us_time();
    // TODO: actual divisibility of seconds
    if (last_rotation < now - dns_fsec_to_us_time(period_sec))
        return 1;
    return 0;
}

const char *dns_output_field_names[] = {
    "timestamp",
    "delay_us",
    "req_dns_len",
    "resp_dns_len",
    "req_net_len",
    "resp_net_len",
    "client_addr",
    "client_port",
    "server_addr",
    "server_port",
    "net_proto",
    "net_ipv",
    "net_ttl",
    "req_udp_sum",
    "id",
    "qtype",
    "qclass",
    "flags",
    "qname",
    "resp_ancount",
    "resp_arcount",
    "resp_nscount",

    NULL,
};

_Static_assert(sizeof(dns_output_field_names) == sizeof(char *) * (dns_of_LAST + 1), "dns_output_field_names and dns_output_field mismatch");

const char *dns_drop_reason_names[] = {
  "other",
  "malformed",
  "fragmented",
  "protocol",
  "bad_dns",
  "limit",
  NULL,
};

