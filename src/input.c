#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>
#include <lz4.h>
#include <lz4frame.h>

#include "common.h"
#include "collector.h"
#include "timeframe.h"
#include "input.h"


static char *
dns_input_conf_init(void *data)
{
    struct dns_input *input = (struct dns_input *) data;
    input->snaplen = -1;
    input->uri = NULL;
    input->bpf_string = NULL;
    input->promisc = 0;
    input->bpf_filter = NULL;

    return NULL;
}


static char *
dns_input_conf_commit(void *data)
{
    struct dns_input *input UNUSED = (struct dns_input *) data;

    return NULL;
}


struct cf_section dns_input_section = {
    CF_TYPE(struct dns_input),
    .init = &dns_input_conf_init,
    .commit = &dns_input_conf_commit,
    CF_ITEMS {
        CF_STRING("device", PTR_TO(struct dns_input, uri)),
        CF_STRING("filter", PTR_TO(struct dns_input, bpf_string)),
        CF_INT("snaplen", PTR_TO(struct dns_input, snaplen)),
        CF_INT("promiscuous", PTR_TO(struct dns_input, promisc)),
        CF_END
    }
};


void
dns_trace_close(dns_collector_t *col)
{
    assert(col && col->trace);

    struct dns_input *input = &col->conf->input;

    if (input && input->bpf_filter) {
        trace_destroy_filter(input->bpf_filter);
        input->bpf_filter = NULL;
    }

    trace_destroy(col->trace);
    col->trace = NULL;
}


dns_ret_t
dns_trace_open(dns_collector_t *col, char *inuri)
{
    assert(col && (!col->trace));

    struct dns_input *input = &col->conf->input;
    int r;

    if (inuri) {
        // offline from a pcap file
        col->trace = trace_create(inuri);
    } else {
        assert(input);
        // configured live input
        col->trace = trace_create(input->uri);
    }
    assert(col->trace);

    if (trace_is_err(col->trace)) {
        if (inuri)
            msg(L_FATAL, "libtrace error opening offline input '%s': %s", inuri, trace_get_err(col->trace).problem);
        else
            msg(L_FATAL, "libtrace error opening live input '%s': %s", input->uri, trace_get_err(col->trace).problem);
        trace_destroy(col->trace);
        col->trace = NULL;
        return DNS_RET_ERR;
    }

    if (inuri) {
        // offline from a pcap file
        col->conf->wait_for_outputs = 1;
        int enable = 1;
        r = trace_config(col->trace, TRACE_OPTION_EVENT_REALTIME, &enable);
        if (r < 0)
            msg(L_ERROR, "libtrace error setting no-wait reading for '%s': %s", inuri, trace_get_err(col->trace).problem);
        return DNS_RET_OK;
    }

    // configure live input

    r = 0;
    if (input->snaplen > 0)
        r = trace_config(col->trace, TRACE_OPTION_SNAPLEN, &input->snaplen);

    if (r == 0)
        r = trace_config(col->trace, TRACE_OPTION_PROMISC, &input->promisc);

    if (r < 0) {
        msg(L_FATAL, "libtrace error configutring live input '%s': %s", input->uri, trace_get_err(col->trace).problem);
        trace_destroy(col->trace);
        col->trace = NULL;
        return DNS_RET_ERR;
    }

    if (input->bpf_string) {
        assert(!input->bpf_filter);

        input->bpf_filter = trace_create_filter(input->bpf_string);
        // no error possible here
        assert(input->bpf_filter); 
    
        r = trace_config(col->trace, TRACE_OPTION_FILTER, input->bpf_filter);

        if (r < 0) {
            msg(L_FATAL, "libtrace error applying filter '%s' to '%s': %s", input->bpf_string, input->uri, trace_get_err(col->trace).problem);
            trace_destroy(col->trace);
            col->trace = NULL;
            return DNS_RET_ERR;
        }

    }

    return DNS_RET_OK;
}


