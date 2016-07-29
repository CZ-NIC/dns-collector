#ifndef DNSCOL_INPUT_H
#define DNSCOL_INPUT_H

/**
 * \file input.h
 * Libtrace input and configuration module.
 */

#include <string.h>
#include <libtrace.h>

#include "config.h"
#include "common.h"
#include "packet.h"

/**
 * Input configuration.
 */
struct dns_input {

    /** Libtrace current input name.
     * Owned by the input, not NULL.
     * Empty string for offline (pcap file input) processing. */
    char *uri;

    /** Length of wire packet capture */
    int snaplen;

    /** Promiscuous mode for the interface */
    int promisc; 

    /** BPF filter string. Owned by the input. */
    char *bpf_string;

    /** Is the input online capture? If so, closes and sends frames based on
     * real time when no packets arrive for alonger time. */
    int online;

    /** Maximum packet frame duration */
    dns_us_time_t frame_max_duration;

    /** Configuration (temp) variable for frame_max_duration */
    double frame_max_duration_sec;

    /** Maximum packet frame size in bytes */
    int frame_max_size;

    /** BPF compiled filter. Owned by the input. */
    libtrace_filter_t *bpf_filter;

    /** Open trace when reading. Owned by the input. */
    libtrace_t *trace;

    /** Packet allocated for the trace. This single packet struct is
     * reused for all packets of this trace. */
    libtrace_packet_t *packet;

    /** Currently filled frame, owned by the input.
     * start_time may be unset before the first frame. */
    struct dns_packet_frame *frame;

    /** Output frame queue, not owned. */
    struct dns_frame_queue *output;
};


/** libUCW config section. */
extern struct cf_section dns_input_section;

/**
 * Allocate and initialize the input, allocate a frame.
 * The input can be configured later, but note that 
 * dns_input_conf_commit is called by UCW config.
 */
struct dns_input *
dns_input_create(struct dns_frame_queue *output);

/**
 * Send the last frame and a finalizing frame with
 * a message to all subsequent processors to exit.
 * Does not deallocate the input struct itself.
 */
void
dns_input_finish(struct dns_input *input);

/**
 * Free the input struct.
 * Call only after dns_input_finish.
 */
void
dns_input_destroy(struct dns_input *input);

/**
 * Opens a live or offline trace and runs the packet processing loop.
 * For online input, input->uri is used and set offline_uri=NULL.
 * For offline input, offline_uri specifies the file to process.
 */
dns_ret_t
dns_input_process(struct dns_input *input, const char *offline_uri);

#endif /* DNSCOL_INPUT_H */
