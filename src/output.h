#ifndef DNSCOL_OUTPUT_H
#define DNSCOL_OUTPUT_H

/**
 * \file output.h
 * Generic output module interface, with subprocess pipes.
 */

#include <string.h>
#include <pthread.h>

#include "common.h"
#include "packet.h"

/**
 * Active output thread base, extended by individual output types.
 */
struct dns_output {
    /** Input queue. */
    struct dns_frame_queue *in;

    /** The thread processing this output. Owned by the output. */
    pthread_t thread;

    /** The mutex indicating that the thread is started and running. */
    pthread_mutex_t running;

    /** Output file descriptor */
    int out_fd;

    /** Output file wrapper struct. */
    FILE *out_file;

    /**
     * Hook to write packet (or packet pair) to the output `out_file` or `out_fd`.
     * Default (when NULL) is just discard.
     * When not using `dns_output_write()`also update `this->wrote_bytes`.
     */
    dns_ret_t (*write_packet)(struct dns_output *out, dns_packet_t *pkt);

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

    /** Output rotation period in seconds. Zero or less means do not rotate.
     * The output is rotated whenever sec_since_midnight is divisible by period. */
    int period_sec;

    /** File opening time. `DNS_NO_TIME` indicates output not open. */
    dns_us_time_t output_opened;
    uint64_t total_items;
    uint64_t total_request_only;
    uint64_t total_response_only;
    uint64_t total_bytes;
    uint64_t current_items;
    uint64_t current_request_only;
    uint64_t current_response_only;
    uint64_t current_bytes;


    /** The time of a last event in the output. */
    dns_us_time_t current_time;


    /** Current file name, not NULL after .._open(), NULL after .._close().
     * Owned by the output. */
    char *path;

    /** Configured path format string. Owned by the output. */
    char *path_fmt;

    /** Configured command to pipe output through, run via `/bin/sh -c "CMD"`.
     * May be NULL (then write directly). The program should read
     * fro stdin and write to stdout, errors from stderr are printed freely. 
     * Each of the output files is postprocessed individually.
     * Example: "gzip -9"
     * Owned by the output. */
    char *pipe_cmd;

    /** PID of the forked output if running, 0 otherwise. */
    pid_t pipe_pid;
};

#define DNS_OUTPUT_FILENAME_EXTRA 64

/**
 * Initialise an already allocated output structure.
 */
void
dns_output_init(struct dns_output *out, struct dns_frame_queue *in, const char *path_fmt, const char *pipe_cmd, int period_sec);

/**
 * Deinitialise the given output, freeing any owned objects.
 * Must be called only after thread stopped and dns_output_finish() was called.
 * Does NOT dealloc the output struct itself.
 */
void
dns_output_finalize(struct dns_output *out);

/**
 * Start the output thread.
 */
void
dns_output_start(struct dns_output *out);

/**
 * Wait for and join the output thread.
 */
void
dns_output_finish(struct dns_output *out);


/**
 * Check and potentionally rotate output files.
 * Opens the output if not open. Rotates output file after `out->period` time
 * has passed since the opening. Requires `time!=DNS_NO_TIME`.
 */
//void
//dns_output_check_rotation(struct dns_output *out, dns_us_time_t time);

/**
 * Open and start new output stream file.
 * Runs a pipe subprocess, or just opens a new file.
 * Calls `out->start_file()` if not NULL.
 */
void
dns_output_open(struct dns_output *out, dns_us_time_t time);

/**
 * Close an output stream or file.
 * Calls `out->finish_file()` and waits for the pipe subprocess, if any.
 */
void 
dns_output_close(struct dns_output *out, dns_us_time_t time);


#endif /* DNSCOL_OUTPUT_H */
