#ifndef DNSCOL_OUTPUT_CSV_H
#define DNSCOL_OUTPUT_CSV_H

/**
 * \file output_csv.h
 * Output to CSV files - configuration and writing.
 */

#include "output.h"
#include "frame_queue.h"
#include "config.h"

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

struct dns_output_csv *
dns_output_csv_create(struct dns_config *conf, struct dns_frame_queue *in);

void
dns_output_csv_start(struct dns_output_csv *out);

void
dns_output_csv_finish(struct dns_output_csv *out);

void
dns_output_csv_destroy(struct dns_output_csv *out);


#endif /* DNSCOL_OUTPUT_CSV_H */


