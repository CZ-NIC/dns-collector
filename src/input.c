#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>

#include "common.h"
#include "packet_frame.h"
#include "frame_queue.h"
#include "input.h"


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
    input->promisc = conf->input_promiscuous;
    input->bpf_string = strdup(conf->input_filter);

    return input;
}

/**
 * Outputs the current frame and creates a new one.
 */

static void
dns_input_output_frame(struct dns_input *input)
{
    assert(input && input->frame);
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
    if (time < input->frame->time_start) {
        msg(L_WARN, "Ignoring advance_time_to time before frame start: %f before (%f - %f)",
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
}


void
dns_input_destroy(struct dns_input *input)
{
    assert(input && !input->trace && !input->packet && !input->frame);

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
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        dns_input_advance_time_to(input, dns_us_time_from_timespec(&now));
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
 * Read and process the next packet from an input trace.
 */
static dns_ret_t
dns_input_read_next_packet(struct dns_input *input)
{
    int r = trace_read_packet(input->trace, input->packet);
    if (r == 0)
        return DNS_RET_EOF;
    if (r < 0) {
        trace_perror(input->trace, "trace_read_packet");
        return DNS_RET_ERR;
    }

    struct dns_packet *pkt = NULL;
    r = dns_packet_create_from_libtrace(input->packet, &pkt);
    if (r != DNS_RET_OK) {
        // TODO: dump and account the invalid packet, use err
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
    
dns_ret_t
dns_input_process(struct dns_input *input, const char *offline_uri)
{
    assert((!!input->online) == (!offline_uri));
    dns_ret_t r;

    if (!input->online) {
        input->uri = (char *)offline_uri;
    }
    msg(L_INFO, "Processing input %s", input->uri);
    
    r = dns_input_trace_open(input);
    if (r != DNS_RET_OK) {
        msg(L_ERROR, "Failed to open %s", input->uri);
        return r;
    }

    while (1) {
        // Naive loop, TODO: event loop and SIGINT handling
        r = dns_input_read_next_packet(input);
        if (r == DNS_RET_EOF) {
            r = DNS_RET_OK;
            break;
        }
        if (r != DNS_RET_OK) {
            break;
        }
    }

    dns_input_trace_close(input);
    return r;
}



