#ifndef DNSCOL_INPUT_H
#define DNSCOL_INPUT_H

/**
 * \file input.h
 * Libtrace input and configuration module.
 */

#include <string.h>
#include <pthread.h>
#include <libtrace.h>

#include "config.h"
#include "common.h"
#include "packet.h"

/**
 * Input configuration.
 */
struct dns_input {

    /** Libtrace input name. Not owned by the input. */
    char *uri;

    /** Length of wire packet capture */
    int snaplen;

    /** Promiscuous mode for the interface */
    int promisc; 

    /** BPF filter string. Not owned by the input. */
    char *bpf_string;

    /** BPF compiled filter. Owned by the input. */
    libtrace_filter_t *bpf_filter;
};


/** libUCW config section. */
extern struct cf_section dns_input_section;


/**
 * Opens a live or offline trace. 
 * If `inuri` is given, the correspnding capture file is open.
 * When `inuri=NULL`, opens the configured input device with its options.
 */
dns_ret_t
dns_trace_open(dns_collector_t *col, char *inuri);

/**
 * Closes a live or offline trace open by `dns_trace_open()`. 
 * Also frees the compiled filter if any.
 */
void
dns_trace_close(dns_collector_t *col);

#endif /* DNSCOL_INPUT_H */
