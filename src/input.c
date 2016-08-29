/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "input.h"

static void
dns_input_report(struct dns_input *input, int force);

struct dns_input *
dns_input_create(struct dns_config *conf, struct dns_frame_queue *output)
{
    struct dns_input *input = xmalloc_zero(sizeof(struct dns_input));

    input->snaplen = conf->input_snaplen;
    input->online = 0;
    input->uri = strdup(conf->input_uri);
    input->output = output;
    input->frame = dns_packet_frame_create(DNS_NO_TIME, DNS_NO_TIME);
    input->frame_max_duration = dns_fsec_to_us_time(conf->max_frame_duration_sec);
    input->frame_max_size = conf->max_frame_size;
    input->real_time_grace = dns_fsec_to_us_time(conf->input_real_time_grace_sec);
    input->promisc = conf->input_promiscuous;
    input->bpf_string = strdup(conf->input_filter);
    input->report_period_sec = conf->report_period_sec;
    input->last_report_time = DNS_NO_TIME;
    if (conf->dump_path_fmt && strlen(conf->dump_path_fmt) > 0) {
        input->dumper = dns_dump_create(conf);
    } else {
        input->dumper = NULL;
    }

    return input;
}

/**
 * Outputs the current frame and creates a new one.
 */

static void
dns_input_output_frame(struct dns_input *input)
{
    assert(input && input->frame);
    dns_input_report(input, 0);
    struct dns_packet_frame *new_frame = dns_packet_frame_create(input->frame->time_end, input->frame->time_end);
    dns_frame_queue_enqueue(input->output, input->frame); // Hand over ownership
    input->frame = new_frame;
}

/**
 * Output packet frames until the given time fits within the current frame.
 * Sets the current frame time if not set yet (DNS_NO_TIME).
 */

static void
dns_input_advance_time_to(struct dns_input *input, dns_us_time_t time)
{
    if (input->frame->time_start == DNS_NO_TIME) {
        input->frame->time_start = time;
        input->frame->time_end = time;
    }
    if (time < input->frame->time_start - 1000) { // TODO: configurable grace time, currently 1 ms
        msg(L_WARN | DNS_MSG_SPAM, "Advance_time_to time before frame start: %f is before (%f - %f)",
            dns_us_time_to_fsec(time), dns_us_time_to_fsec(input->frame->time_start), dns_us_time_to_fsec(input->frame->time_end));
    }
    while (time >= input->frame->time_start + input->frame_max_duration) {
        assert(input->frame->time_end <= input->frame->time_start + input->frame_max_duration);
        input->frame->time_end = input->frame->time_start + input->frame_max_duration;
        dns_input_output_frame(input);
    }
    input->frame->time_end = MAX(input->frame->time_end, time);
}


void
dns_input_finish(struct dns_input *input)
{
    assert(input && input->frame);

    dns_input_output_frame(input);

    // One final empty frame
    input->frame->type = 1;
    dns_frame_queue_enqueue(input->output, input->frame);
    input->frame = NULL;

    if (input->dumper)
        dns_dump_close(input->dumper);
}


void
dns_input_destroy(struct dns_input *input)
{
    assert(input && !input->trace && !input->packet && !input->frame);

    if (input->dumper)
        dns_dump_destroy(input->dumper);
    if (input->bpf_string)
        free(input->bpf_string);
    if (input->uri)
        free(input->uri);
    free(input);
}


/**
 * Actually closes the open trace.
 */

static void
dns_input_trace_close(struct dns_input *input)
{
    assert(input && input->trace && input->packet && input->frame);

    if (input->bpf_filter) {
        trace_destroy_filter(input->bpf_filter);
        input->bpf_filter = NULL;
    }

    trace_destroy(input->trace);
    input->trace = NULL;

    trace_destroy_packet(input->packet);
    input->packet = NULL;
}

/**
 * Actually opens a live or offline trace given by input->uri.
 */

static dns_ret_t
dns_input_trace_open(struct dns_input *input)
{
    assert(input && input->uri && !input->trace && !input->packet);
    int r;

    input->trace = trace_create(input->uri);
    assert(input->trace);

    if (trace_is_err(input->trace)) {
        msg(L_FATAL, "libtrace error opening input '%s': %s", input->uri, trace_get_err(input->trace).problem);
        trace_destroy(input->trace);
        input->trace = NULL;
        return DNS_RET_ERR;
    }

    if (!input->online) {
        // offline from a pcap file
        int enable = 1;
        r = trace_config(input->trace, TRACE_OPTION_EVENT_REALTIME, &enable);
        if (r < 0)
            msg(L_ERROR, "libtrace error setting no-wait reading for '%s': %s", input->uri, trace_get_err(input->trace).problem);
    }

    if (input->online) {
        dns_input_advance_time_to(input, dns_current_us_time());
    }

    r = 0;
    if (input->snaplen > 0)
        r = trace_config(input->trace, TRACE_OPTION_SNAPLEN, &input->snaplen);

    if ((r == 0) && (input->promisc) && (input->online))
        r = trace_config(input->trace, TRACE_OPTION_PROMISC, &input->promisc);

    if (r < 0) {
        msg(L_FATAL, "libtrace error configuring input '%s': %s", input->uri, trace_get_err(input->trace).problem);
        trace_destroy(input->trace);
        input->trace = NULL;
        return DNS_RET_ERR;
    }

    if (input->bpf_string) {
        assert(!input->bpf_filter);

        input->bpf_filter = trace_create_filter(input->bpf_string);
        // NOTE: no error should be possible here
        assert(input->bpf_filter); 
    
        r = trace_config(input->trace, TRACE_OPTION_FILTER, input->bpf_filter);

        if (r < 0) {
            msg(L_FATAL, "libtrace error applying filter '%s' to '%s': %s", input->bpf_string, input->uri, trace_get_err(input->trace).problem);
            trace_destroy(input->trace);
            input->trace = NULL;
            return DNS_RET_ERR;
        }

    }

    r = trace_start(input->trace);
    if (r < 0) {
        msg(L_FATAL, "libtrace_start of %s error: %s", input->uri, trace_get_err(input->trace).problem);
        if (input->bpf_filter) {
            trace_destroy_filter(input->bpf_filter);
            input->bpf_filter = NULL;
        }
        trace_destroy(input->trace);
        input->trace = NULL;
        return DNS_RET_ERR;
    }

    input->packet = trace_create_packet();
    if (!input->packet)
        die("FATAL: libtrace packet allocation error!");

    return DNS_RET_OK;
}

/**
 * Read and process the packet just read from an input trace.
 */
static dns_ret_t
dns_input_process_read_packet(struct dns_input *input)
{
    input->current_packets_read += 1;
    input->current_bytes_read += trace_get_wire_length(input->packet);
    struct dns_packet *pkt = NULL;
    dns_ret_t r = dns_packet_create_from_libtrace(input->packet, &pkt);
    if (r != DNS_RET_OK) {
        if (input->dumper)
            if (dns_dump_packet(input->dumper, input->packet, r) != DNS_RET_OK) {
                msg(L_ERROR, "Dump error (opening or writing), givin up on packet dumping");
                dns_dump_destroy(input->dumper);
                input->dumper = NULL;
            }
        return DNS_RET_OK;
    }
    assert(pkt != NULL);

    dns_input_advance_time_to(input, pkt->ts);
    if ((input->frame->count > 0) && (input->frame->size + pkt->memory_size > input->frame_max_size)) {
        dns_input_output_frame(input);
    }
    dns_packet_frame_append_packet(input->frame, pkt);
    return DNS_RET_OK;
}

/**
 * Advance the time to the real time when the trace is online and
 * the frame time is more than real_time_grace behind real time.
 * Only advances to real_time_grace before now.
 */
static void
dns_input_process_catch_real_time(struct dns_input *input)
{
    if (input->online) {
        dns_us_time_t now_us = dns_current_us_time();
        if (now_us > input->frame->time_end + input->real_time_grace) {
            dns_input_advance_time_to(input, now_us - input->real_time_grace);
        }
    }
}

/**
 * Write a input state report if enough time has passed.
 */
static void
dns_input_report(struct dns_input *input, int force)
{
    dns_us_time_t now = dns_current_us_time();
    if ((!force) && (!dns_next_rotation(input->report_period_sec, input->last_report_time, now))) 
        return;

    // Likely the only half-sane statistic from libtrace :-/
    input->current_packets_dropped = trace_get_dropped_packets(input->trace);
    if (input->current_packets_dropped == UINT64_MAX)
        input->current_packets_dropped = 0;

#define DOSTAT(stat) \
    double rate_ ## stat = 0.0; \
    if (input->last_report_time != DNS_NO_TIME) {\
        rate_ ## stat = (input->current_ ## stat) / dns_us_time_to_fsec(now - input->last_report_time);\
    } \
    input->total_ ## stat += input->current_ ## stat; 

    DOSTAT(packets_read);
    DOSTAT(bytes_read);
    DOSTAT(packets_dropped);

    msg(L_INFO, "input: %"PRIu64" (%.3lg/s) packets, %"PRIu64" (%.3lg/s) bytes, %"PRIu64" (%.3lg/s) dropped",
        input->current_packets_read, rate_packets_read,
        input->current_bytes_read, rate_bytes_read,
        input->current_packets_dropped, rate_packets_dropped);
    msg(L_INFO, "input totals: %"PRIu64" packets, %"PRIu64" bytes, %"PRIu64" dropped",
        input->total_packets_read, input->total_bytes_read, input->total_packets_dropped);
    if (input->online && input->frame) {
        msg(L_INFO, "input is %.3lf s behind real time (with %.3lf s grace time)",
            dns_us_time_to_fsec(now - input->frame->time_end),
            dns_us_time_to_fsec(input->real_time_grace + input->frame_max_duration));
    }

    input->current_packets_read = 0;
    input->current_bytes_read = 0;
    input->current_packets_dropped = 0;

    input->last_report_time = now;
#undef DOSTAT
}

dns_ret_t
dns_input_process(struct dns_input *input, const char *offline_uri)
{
    assert((!!input->online) == (!offline_uri));

    dns_ret_t r;
    libtrace_eventobj_t ev;
    fd_set rfds;

    if (!input->online) {
        if (input->uri)
            free(input->uri);
        input->uri = strdup(offline_uri);
        msg(L_INFO, "Processing offline input %s", input->uri);
    } else {
        msg(L_INFO, "Processing online input %s", input->uri);
    }
    
    r = dns_input_trace_open(input);
    if (r != DNS_RET_OK) {
        msg(L_ERROR, "Failed to open %s", input->uri);
        return r;
    }

    // Do not report immediatelly
    if (input->last_report_time == DNS_NO_TIME)
        input->last_report_time = dns_current_us_time();

    // Wake up at least twice per timeframe or once per second
    dns_us_time_t max_sleep = MIN(input->frame_max_duration, dns_fsec_to_us_time(1.0));

    // Online event loop
    while (1) {

        if (dns_global_stop) {
            msg(L_INFO, "Interrupted reading input %s", input->uri);
            dns_input_report(input, 1);
            dns_input_trace_close(input);
            return DNS_RET_OK;
        }

        ev = trace_event(input->trace, input->packet);
        switch (ev.type) {

        case TRACE_EVENT_PACKET:
            dns_input_process_read_packet(input);
            break;

        case TRACE_EVENT_TERMINATE:
            msg(L_DEBUG, "Reading '%s' terminated", input->uri);
            if (trace_is_err(input->trace)) {
                trace_perror(input->trace, "trace terminated with error");
                r = DNS_RET_ERR;
            } else {
                r = DNS_RET_OK;
            }
            dns_input_report(input, 1);
            dns_input_trace_close(input);
            return r;

        case TRACE_EVENT_SLEEP:
            dns_input_process_catch_real_time(input);
            usleep((useconds_t)(MIN(max_sleep, dns_fsec_to_us_time(ev.seconds))));
            break;

        case TRACE_EVENT_IOWAIT:
            dns_input_process_catch_real_time(input);
            FD_ZERO(&rfds);
            FD_SET(ev.fd, &rfds);
            struct timeval tv = { .tv_sec = max_sleep / 1000000, .tv_usec = max_sleep % 1000000};
            int sr = select(ev.fd + 1, &rfds, NULL, NULL, &tv);
            if (sr < 0) {
                if (errno != EINTR) {
                    perror("select on trace");
                    dns_input_trace_close(input);
                    return DNS_RET_ERR;
                }
            }
            break;

        default:
            die("Unknown trace event");
        }
    }
    die("Edge of the world!");
}



