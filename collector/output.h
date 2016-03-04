#ifndef DNSCOL_OUTPUT_H
#define DNSCOL_OUTPUT_H

/**
 * \file output.h
 * Generic output module interface, plus compression.
 */

#include <string.h>
#include <pthread.h>
#include <lz4frame.h>

#include "config.h"
#include "common.h"
#include "packet.h"

/**
 * Common libucw output config options
 */

#define CF_DNS_OUTPUT_COMMON \
        CF_STRING("path", PTR_TO(struct dns_output, path_fmt)), \
        CF_DOUBLE("period", PTR_TO(struct dns_output, period_sec)), \
        CF_LOOKUP("compression", PTR_TO(struct dns_output, compression), dns_output_compression_names)


/**
 * Output thread stop flag values.
 */
enum dns_output_stop {
    dns_os_none = 0,
    dns_os_queue,
    dns_os_frame,
};


/**
 * Output configuration and active output entry.
 */
struct dns_output {
    /**
     * LibUCW circular list node header. Read-only after init.
     */
    cnode n;

    /**
     * The thread processing this output. Not owned by the output.
     * Accessed by the collector without locked mutex.
     */
    pthread_t thread;

    /**
     * The condition to wait on for new timeframes and stop conditions.
     * Shared with the collector and other outputs.
     */
    pthread_cond_t *collector_output_cond;

    /** Condition signalled by the output when popping a frame from a full
     * queue to signal the collector that pushing the next frame may be possible.
     * Used in offline pcap processing only, may be NULL.
     */
    pthread_cond_t *collector_unblock_cond;

    /**
     * Collector mutex for access into output shared vars and timeframe
     * refcounting. Shared with the collector and other outputs.
     */
    pthread_mutex_t *collector_mutex;

    /** @{ @name Elements accessible only with locked collector mutex. */
    /**
     * Array of queued frame pointers. queue[0] is the oldest frame.
     * Access only with locked collector mutex.
     */
    struct dns_timeframe **queue;
    /**
     * Current queue length.
     * Access only with locked collector mutex.
     */
    int queue_len;
    /**
     * Current queue length.
     * Access only with locked collector mutex.
     */
    int max_queue_len;
    /**
     * Current queue length.
     * Access only with locked collector mutex.
     */
    enum dns_output_stop stop_flag;
    /** @} */


    /**
     * Hook to write packet (or packet pair) to the output.
     * Default (when NULL) is none.
     * Should increment `this->wrote_items`. When not using `dns_output_write()`,
     * also update `this->wrote_bytes_compressed` and `this->wrote_bytes`.
     */
    dns_ret_t (*write_packet)(struct dns_output *out, dns_packet_t *pkt);

    /**
     * Hook called to open new file and initialise compression.
     * The default (when NULL) is `dns_output_open()`.
     */
    void (*open_file)(struct dns_output *out, dns_us_time_t time);

    /**
     * Hook called after output file initialisation. Use it to write headers etc.
     * Default (when NULL) is none.
     */
    void (*start_file)(struct dns_output *out, dns_us_time_t time);

    /**
     * Hook called before closing of an output file. Use it to write footers etc.
     * Default (when NULL) is none.
     */
    void (*finish_file)(struct dns_output *out, dns_us_time_t time);

    /**
     * Hook called to close an output file and finalise/flush compression.
     * Default (when NULL) is `dns_output_close()`.
     */
    void (*close_file)(struct dns_output *out, dns_us_time_t time);

    
    /** Configured file rotation period. Zero or less means do not rotate */
    double period_sec;

    /** Rotation period in micro seconds, computed from `period_sec`. Zero means do not rotate */
    dns_us_time_t period;

    /** Open file stream */
    FILE *f;

    /** File opening time. `DNS_NO_TIME` indicates output not open. */
    dns_us_time_t output_opened;

    /** Allocated file name, may be `NULL`. Owned by the output. */
    char *path;

    /**
     * Configured path format string. Owned by the output.
     * Read-only access for both the output thread and 
     * the collector.
     */
    char *path_fmt;


    /** Compression setting (`enum dns_output_compressions`) */
    int compression; 

    /**@{ @name Compression LZ4 settings and buffers */
    // TODO: Refactor / move into a substructure?
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
     * The hook `write_packet` must update this.*/
    size_t wrote_items;
};


/**
 * Helper to init general output configuration `struct dns_output` (for the UCW conf system).
 */
char *
dns_output_conf_init(struct dns_output *out);


/**
 * Helper to post-process general output configuration `struct dns_output` (for the UCW conf system).
 */
char *
dns_output_conf_commit(struct dns_output *out);


/**@{ @name Config sections for particular output types */
#ifdef DNS_WITH_CSV
extern struct cf_section dns_output_csv_section;
#endif // DNS_WITH_CSV
#ifdef DNS_WITH_PROTO
extern struct cf_section dns_output_proto_section;
#endif // DNS_WITH_PROTO
#ifdef DNS_WITH_CBOR
extern struct cf_section dns_output_cbor_section;
#endif // DNS_WITH_CBOR
/**@} */


/**
 * Initialise the output from the given collector.
 */
void
dns_output_init(struct dns_output *out, struct dns_collector *col);


/**
 * Deinitialise the given output. Must be called only after all output threads exited.
 * Does NOT dealloc the output itself, as it was probably allocated by the config system.
 */
void
dns_output_destroy(struct dns_output *out);


/**
 * Main output thread routine.
 * Must be called after `dns_output_init()`.
 * Takes `struct dns_output *`, returns `NULL`.
 */
void *
dns_output_thread_main(void *data);


/** @{ @name Open file opening, closing and rotation */

/**
 * Allowed rounding inaccuracy [microseconds] when checking output file rotation on frame rotation.
 */
#define DNS_OUTPUT_ROTATION_GRACE_US 10000

/**
 * Check and potentionally rotate output files.
 * Opens the output if not open. Rotates output file after `out->period` time
 * has passed since the opening. Requires `time!=DNS_NO_TIME`.
 */
void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time);

/**
 * Open and start new output stream file.
 * Calls `out->open_file()` or `dns_output_open_file()`, then initializes compression,
 * then `out->start_file()`.
 */
void
dns_output_open(struct dns_output *out, dns_us_time_t time);

/**
 * Close an output stream or file.
 * Calls `out->finish_file()`, then finishes compression, then
 * `out->close_file()` or `dns_output_close_file()`
 */
void 
dns_output_close(struct dns_output *out, dns_us_time_t time);

/** @} */

/**
 * Open a file by expanding `path`. Default for `out->open_file()` hook.
 */
void
dns_output_open_file(struct dns_output *out, dns_us_time_t time);

/**
 * Cloase a file. Default for `out->close_file()` hook.
 */
void
dns_output_close_file(struct dns_output *out, dns_us_time_t time);


/** @{ @name Timeframe queue manipulation */
/**
 * Pop and return the oldest frame from output's queue.
 * Returns NULL when the queue is empty.
 * The caller must hold the collector mutex.
 * The caller must decref the frame when done processing it.
 */
struct dns_timeframe *
dns_output_pop_frame(struct dns_output *out);


/**
 * Insert a newest frame into the output queue.
 * The collector calling this must hold the collector mutex.
 * Drops the oldest frame if queue full.
 */
void
dns_output_push_frame(struct dns_output *out, struct dns_timeframe *tf);


/**
 * Write all frame queries into the output.
 * May block on IO. Does not rotate files.
 */
void
dns_output_write_frame(struct dns_output *out, struct dns_timeframe *tf);

/** @} */


#endif /* DNSCOL_OUTPUT_H */
