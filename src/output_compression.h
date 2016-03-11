#ifndef DNSCOL_OUTPUT_COMPRESSION_H
#define DNSCOL_OUTPUT_COMPRESSION_H

#include <lz4frame.h>
#include "common.h"

/**
 * Output compression types
 */

enum dns_output_compressions {
    dns_oc_none = 0,
    dns_oc_lz4fast,
    dns_oc_lz4med,
    dns_oc_lz4best,
    dns_oc_LAST, // Sentinel
};

/** Output compression type names */

extern const char *dns_output_compression_names[];

/**
 * Internal size of write block when compressing.
 */
#define DNS_OUTPUT_LZ4_WRITE_SIZE (1024 * 4)

#define LZ4_HEADER_SIZE 20
#define LZ4_FOOTER_SIZE 8

/**
 * Initialize the selected compression on a newly-open file.
 */
void
dns_output_start_compression(struct dns_output *out);

/**
 * Finalise (and deallocate) compression on a file just before closing.
 */
void
dns_output_finish_compression(struct dns_output *out);

/**
 * Write a data block to an output file, handling compression.
 * Ignores IO errors (just logs them).
 */
void
dns_output_write(struct dns_output *out, const char *buf, size_t len);


#endif // DNSCOL_OUTPUT_COMPRESSION_H

