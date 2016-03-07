
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common.h"


dns_us_time_t
dns_us_time_from_timeval(const struct timeval *t)
{
    return t->tv_usec + (1000000 * t->tv_sec);
}


dns_us_time_t
dns_us_time_from_timespec(const struct timespec *t)
{
    return (t->tv_nsec / 1000) + (1000000 * t->tv_sec);
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
    "flags",

    "client_addr",
    "client_port",
    "server_addr",
    "server_port",

    "id",
    "qname",
    "qtype",
    "qclass",

    "request_time_us",
    "request_flags",
    "request_ans_rrs",
    "request_auth_rrs",
    "request_add_rrs",
    "request_length",

    "response_time_us",
    "response_flags",
    "response_ans_rrs",
    "response_auth_rrs",
    "response_add_rrs",
    "response_length",

    NULL,
};

const char *dns_drop_reason_names[] = {
  "other",
  "malformed",
  "fragmented",
  "protocol",
  "bad_dns",
  "limit",
  NULL,
};

