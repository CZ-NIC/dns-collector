
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

void dns_die(const char *msg)
{
    fprintf(stderr, "FATAL: %s\n", msg);
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
