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
    /** Libucw clist member */
    cnode n;

    /** Libtrace input name. Not owned by the input. */
    char *uri;

    /** Length of wire packet capture */
    int snaplen;

    /** Promiscuous mode for the interface */
    int promisc; 

    /** BPF filter string. Not owned by the input. */
    char *bpf_string;

    /** Is the input offline pcap? */
    int offline;

    /** BPF compiled filter. Owned by the input. */
    libtrace_filter_t *bpf_filter;

    /** Open trace when reading. Owned by the input. */
    libtrace_t *trace;

    /** Packet allocated for the trace. This single packet struct is
     * used for all packets of this trace. */
    libtrace_packet_t *packet;
};


/** libUCW config section. */
extern struct cf_section dns_input_section;


/**
 * Opens a live or offline trace for given configured input. 
 */
dns_ret_t
dns_input_open(struct dns_input *input);

/**
 * Closes a live or offline trace open by `dns_input_open()`. 
 * Also frees the packet and compiled filter if any.
 */
void
dns_input_close(struct dns_input *input);

#endif /* DNSCOL_INPUT_H */
