#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <errno.h>
#include <libtrace.h>
#include <unistd.h>

#include "common.h"
#include "packet_frame.h"
#include "frame_queue.h"
#include "dump.h"
#include "output.h"

struct dns_dump *
dns_dump_create(struct dns_config *conf)
{
    struct dns_dump *dump = xmalloc_zero(sizeof(struct dns_dump));
    dump->period_sec = conf->dump_period_sec;
    dump->path_fmt = strdup(conf->dump_path_fmt);
    dump->uri = NULL;
    dump->rate = conf->dump_rate_limit;
    dump->tokens = 2 * conf->dump_rate_limit;
    dump->last_event = DNS_NO_TIME;
    dump->current_dumped = 0;
    dump->current_bytes = 0;
    dump->current_skipped = 0;
    dump->compress_level = conf->dump_compress_level;
    dump->compress_type = dns_dump_compress_types_num[conf->dump_compress_type];
    return dump;
}

void
dns_dump_close(struct dns_dump *dump)
{
    msg(L_INFO, "dump: Wrote %"PRIu64" packets (%"PRIu64" bytes) to %s, skipped %"PRIu64" packets (rate-limited)",
        dump->current_dumped, dump->current_bytes, dump->uri, dump->current_skipped);
    trace_destroy_output(dump->trace);
    dump->trace = NULL;
    free(dump->uri);
}

dns_ret_t
dns_dump_open(struct dns_dump *dump, dns_us_time_t time)
{
    assert(dump && !dump->trace && dump->path_fmt);
    // Extra space for expansion -- note that most used conversions are "in place": "%d" -> "01" 
    if (dump->uri)
        free(dump->uri);
    int path_len = strlen(dump->path_fmt) + DNS_OUTPUT_FILENAME_EXTRA + 6;
    dump->uri = xmalloc(path_len);
    char *p = dump->uri;
    p += snprintf(p, path_len, "pcap:");
    size_t l = dns_us_time_strftime(p, path_len - (p - dump->uri), dump->path_fmt, time);
    if (l == 0) {
        die("Expanded filename '%s' expansion too long.", dump->path_fmt);
    }

    dump->trace = trace_create_output(dump->uri);
    if (trace_is_err_output(dump->trace)) {
        msg(L_FATAL, "libtrace error opening dump '%s': %s", dump->uri, trace_get_err_output(dump->trace).problem);
        trace_destroy_output(dump->trace);
        dump->trace = NULL;
        return DNS_RET_ERR;
    }

    // Compression
    if ((dump->compress_level > 0) && (dump->compress_type != TRACE_OPTION_COMPRESSTYPE_NONE)) {
        int r = trace_config_output(dump->trace, TRACE_OPTION_OUTPUT_COMPRESSTYPE, &(dump->compress_type));
        if (r >= 0)
            r = trace_config_output(dump->trace, TRACE_OPTION_OUTPUT_COMPRESS, &(dump->compress_level));
        if (r < 0) {
            msg(L_FATAL, "libtrace error setting compression for dump '%s': %s", dump->uri, trace_get_err_output(dump->trace).problem);
            trace_destroy_output(dump->trace);
            dump->trace = NULL;
            return DNS_RET_ERR;
        }
    }

    dump->tokens = MAX(2 * dump->rate, 2.0);
    dump->last_event = time;
    dump->dump_opened = time;
    dump->current_dumped = 0;
    dump->current_skipped = 0;
    dump->current_bytes = 0;

    trace_start_output(dump->trace);

    return DNS_RET_OK;
}

void
dns_dump_destroy(struct dns_dump *dump)
{
    assert(dump && !dump->trace);

    if (dump->uri)
        free(dump->uri);
    free(dump->path_fmt);
    free(dump);
}

dns_ret_t
dns_dump_packet(struct dns_dump *dump, libtrace_packet_t *packet, dns_ret_t reason UNUSED)
{
    assert(dump && packet);

    struct timespec ts = trace_get_timespec(packet);
    dns_us_time_t time = dns_us_time_from_timespec(&ts);
    if (dump->last_event == DNS_NO_TIME)
        dump->last_event = time;
    time = MAX(time, dump->last_event);

    // Rate limit
    if (dump->rate > 1e-6) {
        dump->tokens += dump->rate * dns_us_time_to_fsec(time - dump->last_event);
        dump->last_event = time;
        dump->tokens = MIN(dump->tokens, MAX(2 * dump->rate, 2.0));
        if (dump->tokens < 0.0) {
            dump->current_skipped ++;
            return DNS_RET_OK;
        }
    }

    // Rotation and opening of a new dump file
    if (dump->trace && dns_next_rotation(dump->period_sec, dump->dump_opened, time)) {
       dns_dump_close(dump);
    }
    if (!dump->trace) {
        int r = dns_dump_open(dump, time);
        if (r != DNS_RET_OK)
            return r;
    }

    // Dump
    int r = trace_write_packet(dump->trace, packet);
    if (r <= 0) {
        msg(L_ERROR | DNS_MSG_SPAM, "libtrace error dumping packet to %s: %s",
            dump->uri, trace_get_err_output(dump->trace).problem);
        return DNS_RET_ERR;
    }
    dump->current_dumped ++;
    dump->current_bytes += r;
    dump->tokens -= r;
    // TODO: use the reason?

    return DNS_RET_OK;
}
