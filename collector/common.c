
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

void dns_die_f(int line, const char *file, const char *func, const char *msg)
{
    fprintf(stderr, "FATAL [%s:%d] (%s): %s\n", file, line, func, msg);
    exit(1);
}

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
    return ((double)t) / 1000000.0;
}

dns_us_time_t
dns_fsec_to_us_time(double s)
{
    return (dns_us_time_t)(s * 1000000.0);
}

