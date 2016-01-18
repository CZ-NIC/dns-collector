#ifndef DNSCOL_TIMEFRAME_H
#define DNSCOL_TIMEFRAME_H

#include "common.h"

#include <time.h>

struct dns_timeframe {
    struct timespec time_start;
    struct timespec time_end;
    char name[DNSCOL_MAX_FNAME_LEN];
    // dns_request *dns_requests; // linked list of reuest
    // memory pool
    // status structure
    // request index by (IP, PORT,(DNS-ID))
};

#endif /* DNSCOL_TIMEFRAME_H */
