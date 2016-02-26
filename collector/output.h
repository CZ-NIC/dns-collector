#ifndef DNSCOL_OUTPUT_H
#define DNSCOL_OUTPUT_H

#include <string.h>
#include <ucw/lib.h>
#include <lz4frame.h>

#include "config.h"
#include "common.h"
#include "packet.h"


/** Output compression types */

enum dns_output_compressions {
    dns_oc_none = 0,
    dns_oc_lz4fast,
    dns_oc_lz4med,
    dns_oc_lz4best,
    dns_oc_LAST, // Sentinel
};

/** Output compression type names */

extern const char *dns_output_compression_names[];

#define CF_DNS_OUTPUT_COMMON \
        CF_STRING("path_template", PTR_TO(struct dns_output, path_template)), \
        CF_DOUBLE("period", PTR_TO(struct dns_output, period_sec)), \
        CF_LOOKUP("compression", PTR_TO(struct dns_output, compression), dns_output_compression_names)

/**
 * Output configuration and active output entry.
 */
struct dns_output {
    cnode outputs_list;
    struct dns_collector *col;

    dns_ret_t (*write_packet)(struct dns_output *out, dns_packet_t *pkt);
    dns_ret_t (*drop_packet)(struct dns_output *out, dns_packet_t *pkt, enum dns_drop_reason reason);
    void (*start_file)(struct dns_output *out, dns_us_time_t time);
    void (*finish_file)(struct dns_output *out, dns_us_time_t time);
    int manage_files;

    /** Configured file rotation period. Zero or less means do not rotate */
    double period_sec;
    /** Computed from `period_sec`. Zero means do not rotate */
    dns_us_time_t period;

    /** Open file stream */
    FILE *f;
    /** Configured path format string */
    char *path_template;
    /** Allocated file name when !=NULL, owned by the output */
    char *fname;
    /** File opening time */
    dns_us_time_t f_time_opened;

    /** Compression */
    int compression; // enum dns_output_compressions 
    LZ4F_compressionContext_t lz4_ctx;
    LZ4F_preferences_t *lz4_prefs;
    char *lz4_buf;
    size_t lz4_buf_len;
    size_t lz4_buf_offset;

    /** The number of bytes outputted before compression. */
    size_t wrote_bytes;
    /** Total number of bytes written after compression. */
    size_t wrote_bytes_compressed;
    /** Number of items (packets or query pairs) outputted.
     * Specific `write_packet` and `drop_packet` must update this.*/
    size_t wrote_items;
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
extern struct cf_section dns_output_pcap_section;
extern struct cf_section dns_output_proto_section;


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
 * Allowed inaccuracy [microsec] when checking output rotation on frame rotation.
 */
#define DNS_OUTPUT_ROTATION_GRACE_US 10000

/**
 * Check and potentionally rotate output files.
 *
 * Opens the output if not open. Rotates output filea after `out->period` time
 * has passed since the opening. Only valid for outputs with `manage_files`.
 * Requires `time!=DNS_NO_TIME`.
 */
void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time);

/**
 * Write a data block to an output file, handling compression.
 *
 * TODO: Currently may block, also ignores errors (just logs them).
 */
void
dns_output_write(struct dns_output *out, const char *buf, size_t len);


#endif /* DNSCOL_OUTPUT_H */
