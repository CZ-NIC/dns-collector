#ifndef DNSCOL_OUTPUT_H
#define DNSCOL_OUTPUT_H

/**
 * \file output.h
 * Generic output module interface plus compression.
 */

#include <string.h>
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

/** Common libucw output config options */

#define CF_DNS_OUTPUT_COMMON \
        CF_STRING("path_template", PTR_TO(struct dns_output, path_template)), \
        CF_DOUBLE("period", PTR_TO(struct dns_output, period_sec)), \
        CF_LOOKUP("compression", PTR_TO(struct dns_output, compression), dns_output_compression_names)

/**
 * Output configuration and active output entry.
 */
struct dns_output {
    /** Member of a linked list of outputs */
    cnode outputs_list;
    /** Associated collector */
    struct dns_collector *col;

    /** Hook to write packet (or packet pair) to the output */
    dns_ret_t (*write_packet)(struct dns_output *out, dns_packet_t *pkt);
    /** Hook called when a packet is dropped */
    dns_ret_t (*drop_packet)(struct dns_output *out, dns_packet_t *pkt, enum dns_drop_reason reason);
    /** Hook called on new output file creation */
    void (*start_file)(struct dns_output *out, dns_us_time_t time);
    /** Hook called before closing of an output file */
    void (*finish_file)(struct dns_output *out, dns_us_time_t time);

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

    /** Compression setting (`enum dns_output_compressions`) */
    int compression; 

    /**@{ @name Compression LZ4 settings and buffers */
    LZ4F_compressionContext_t lz4_ctx;
    LZ4F_preferences_t *lz4_prefs;
    char *lz4_buf;
    size_t lz4_buf_len;
    size_t lz4_buf_offset;
    /**@}*/

    /** The number of bytes of output before compression. */
    size_t wrote_bytes;
    /** Total number of bytes of output after compression. */
    size_t wrote_bytes_compressed;
    /** Number of items (packets or query pairs) of output.
     * The hooks `write_packet` and `drop_packet` must update this.*/
    size_t wrote_items;
};

/**
 * Helper to init general output configuration `struct dns_output` (for the UCW conf system).
 */
char *
dns_output_init(struct dns_output *out);

/**
 * Helper to post-process general output configuration `struct dns_output` (for the UCW conf system).
 */
char *
dns_output_commit(struct dns_output *out);

/**@{ @name Config sections for particular output types */
extern struct cf_section dns_output_csv_section;
extern struct cf_section dns_output_pcap_section;
extern struct cf_section dns_output_proto_section;
/**@} */


/**
 * Open an output file following the output filename template.
 * You should not need to call it manually, called on file rotation.
 * Must be called with `time!=DNS_NO_TIME`.
 */
void
dns_output_open(struct dns_output *out, dns_us_time_t time);


/**
 * Close an output file, calling the finalizer if set.
 * You should not need to call it manually, except to force file rotation.
 * Must be called with `time!=DNS_NO_TIME`.
 */
void 
dns_output_close(struct dns_output *out, dns_us_time_t time);

/**
 * Allowed rounding inaccuracy [microseconds] when checking output file rotation on frame rotation.
 */
#define DNS_OUTPUT_ROTATION_GRACE_US 10000

/**
 * Check and potentionally rotate output files.
 * Opens the output if not open. Rotates output file after `out->period` time
 * has passed since the opening. Only valid for outputs with `manage_files`.
 * Requires `time!=DNS_NO_TIME`.
 */
void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time);

/**
 * Write a data block to an output file, handling compression.
 * TODO: Currently may block (move the whole writeout to a separate thread),
 * also ignores errors (just logs them).
 */
void
dns_output_write(struct dns_output *out, const char *buf, size_t len);


#endif /* DNSCOL_OUTPUT_H */
