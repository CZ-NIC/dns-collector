#ifndef DNSCOL_OUTPUT_H
#define DNSCOL_OUTPUT_H

#include <string.h>
#include <ucw/lib.h>
#include <ucw/conf.h>

#include "common.h"
#include "packet.h"

/**
 * Output configuration and active output entry.
 */
struct dns_output {
    cnode outputs_list;

    dns_ret_t (*write_packet)(struct dns_output *out, dns_packet_t *pkt);
    dns_ret_t (*drop_packet)(struct dns_output *out, dns_packet_t *pkt);
    void (*start_file)(struct dns_output *out, dns_us_time_t time);
    void (*finish_file)(struct dns_output *out, dns_us_time_t time);
    int manage_files;

    char *path_template;
    double period_sec;
    dns_us_time_t period;
    FILE *f;
    dns_us_time_t f_time_opened;
};

/**
 * Helper to init general output configuration (for the UCW conf system).
 */
char *
dns_output_init(struct dns_output *out);

/**
 * Helper to post-process general output configuration (for the UCW conf system).
 */
char *
dns_output_commit(struct dns_output *out);


extern struct cf_section dns_output_csv_section;
//extern const struct cf_section dns_output_protobuf_section;
//extern const struct cf_section dns_output_dump_section;


/**
 * Open an output file following the output filename template.
 *
 * You should not need to call it manually.
 * Must be called with `time!=DNS_NO_TIME`.
 */
void
dns_output_open(struct dns_output *out, dns_us_time_t time);


/**
 * Close an output file, calling the finalizer if set.
 *
 * You should not need to call it manually, except to force file rotation.
 * Must be called with `time!=DNS_NO_TIME`.
 */
void 
dns_output_close(struct dns_output *out, dns_us_time_t time);


/**
 * Write a data block to an output file, handling compression, opening, file rotation etc.
 *
 * Opens the output if not open. Rotates output filea after `out->period` time has passed since the opening.
 * No file rotation happens when `time=DNS_NO_TIME` or within a sequence with the same `time`.
 * Only valid for outputs with `manage_files`.
 * TODO: Currently may block, on an error just logs it.
 */
void
dns_output_write(struct dns_output *out, const char *buf, size_t len, dns_us_time_t time);


enum dns_output_field {
    dns_of_flags = 0,
    dns_of_client_addr,
    dns_of_client_port,
    dns_of_server_addr,
    dns_of_server_port,
    dns_of_id,
    dns_of_qname,
    dns_of_qtype,
    dns_of_qclass,
    dns_of_request_time_us,
    dns_of_request_flags,
    dns_of_request_length,
    dns_of_response_time_us,
    dns_of_response_flags,
    dns_of_response_length,
    dns_of_LAST, // Sentinel
};

extern const char *dns_output_field_names[];



#endif /* DNSCOL_OUTPUT_H */
