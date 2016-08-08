#ifndef DNSCOL_OUTPUT_CSV_H
#define DNSCOL_OUTPUT_CSV_H

/**
 * \file output_csv.h
 * Output to CSV files - configuration and writing.
 */

#include "output.h"
#include "frame_queue.h"

/**
 * Configuration structure extending `struct dns_output`.
 */
struct dns_output_csv {
    struct dns_output base;

    /* Separator character */
    int separator;

    /* Write header as the first line of the CSV */
    int inline_header;

    /* Write header into a separate file if not NULL,
     * owned by output */
    char *external_header_path_fmt;

    /* Bitmask with CSV fields to write */
    uint32_t csv_fields;
};

/**
 * CSV output configuration structure.
 * All values owned by whoever placed them there
 * (most likely by the libucw config subsystem).
 */

struct dns_output_csv_config {
    char *path_fmt;
    char *pipe_cmd;
    int period_sec;
    char *separator;
    int inline_header;
    char *external_header_path_fmt;
    uint32_t csv_fields;
};

struct dns_output_csv *
dns_output_csv_create(struct dns_frame_queue *in, const struct dns_output_csv_config *conf);

void
dns_output_csv_start(struct dns_output_csv *out);

void
dns_output_csv_finish(struct dns_output_csv *out);

void
dns_output_csv_destroy(struct dns_output_csv *out);

/**
 * Libucw configuration subsection definition.
 */
extern struct cf_section dns_output_csv_section;

#endif /* DNSCOL_OUTPUT_CSV_H */


