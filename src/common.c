#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <execinfo.h>
#include <arpa/inet.h>

#include "common.h"

int dns_global_stop = 0;

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

const char *dns_output_field_names[] = {
    "timestamp",

    "client_addr",
    "client_port",
    "server_addr",
    "server_port",

    "id",
    "qname",
    "qtype",
    "qclass",
    "flags",

    "request_ans_rrs",
    "request_auth_rrs",
    "request_add_rrs",
    "request_length",

    "response_ans_rrs",
    "response_auth_rrs",
    "response_add_rrs",
    "response_length",

    "delay_us",

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

