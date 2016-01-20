
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "timeframe.h"

dns_timeframe_t *
dns_timeframe_create(const struct timespec *time_start, const char *frame_name) 
{
    dns_timeframe_t *frame = (dns_timeframe_t*) calloc(sizeof(dns_timeframe_t), 1);
    if (!frame) return NULL;

    if (time_start) {
        frame->time_start.tv_sec = time_start->tv_sec;
        frame->time_start.tv_nsec = time_start->tv_nsec;
    } else {
        clock_gettime(CLOCK_REALTIME, &(frame->time_start));
    }

    strncpy(frame->name, frame_name, DNSCOL_MAX_FNAME_LEN);
    frame->name[DNSCOL_MAX_FNAME_LEN - 1] = 0;

    return frame;
}

void
dns_timeframe_writeout(dns_timeframe_t *frame)
{
//    if (frame->time_end) 
}


