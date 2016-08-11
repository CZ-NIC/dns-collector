#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>


#include "common.h"
#include "packet_frame.h"
#include "packet.h"
#include "frame_queue.h"
#include "output.h"


void
dns_output_init(struct dns_output *out, struct dns_frame_queue *in, const char *path_fmt, const char *pipe_cmd, int period_sec)
{
    assert(out);
    bzero(out, sizeof(struct dns_output));
    out->output_opened = DNS_NO_TIME;
    out->current_time = DNS_NO_TIME;
    out->in = in;
    out->path_fmt = strdup(path_fmt ? path_fmt : "");
    out->pipe_cmd = strdup(pipe_cmd ? pipe_cmd : "");
    out->period_sec = period_sec;
}


void
dns_output_finalize(struct dns_output *out)
{
    assert(out && (!out->path));

    if (pthread_mutex_trylock(&out->running) != 0)
	die("destroying a running output");
    pthread_mutex_unlock(&out->running);
    pthread_mutex_destroy(&out->running);
    if (out->path_fmt)
        free(out->path_fmt);
    if (out->pipe_cmd)
        free(out->pipe_cmd);
}

/**
 * Check the status of the output pipe subprocess.
 * With wait actually waits for the process,
 * with wait 0 just checks and returns immeditelly.
 * Returns 0 when not started at all or still running.
 * Returns 1 when exited successfully.
 * Returns 2 when exited unsuccessfully or abnormally.
 */
static int
dns_output_wait_for_pipe_process(struct dns_output *out, int wait)
{
    if (out->pipe_pid <= 0)
        return 0;

    int status;
    pid_t pid = waitpid(out->pipe_pid, &status, wait ? 0 : WNOHANG);
    if (pid < 0) {
        perror("output pipe subprocess error");
        die("waitpid() error");
    }
    if (pid == 0) {
        return 0;
    }
    assert(out->pipe_pid == pid);
    out->pipe_pid = 0;

    if (!WIFEXITED(status)) {
        msg(L_ERROR, "Abnormal pipe subprocess termination.");
        return 2;
    }
    if (WEXITSTATUS(status) != 0) {
        msg(L_ERROR, "Pipe subprocess exited with error status %d.", WEXITSTATUS(status));
        return 2;
    }
    return 1;
}

/**
 * Spawn a subprocess running "/bin/sh" "-c" "sh_cmd".
 * The output of the subprocess is attached to stdout or file named outfile (if given).
 * If outfile is not NULL or "", the file is open (creating or truncating it).
 *
 * The input of the subprocess is attached to a new fd, which is returned.
 * No recoverable error can happen in the master process.
 *
 * On failure in the subprocess (e.g. invalid command, file open error, ...),
 * you have to check it via waitpid() later, e.g. use dns_output_wait_for_pipe_process().
 */
static int
dns_output_start_subprocess(const char *sh_cmd, const char *outfile, pid_t *pidp)
{
    int pipefds[2];
    if (pipe(pipefds) != 0) {
        perror("pipe in dns_output_start_subprocess()");
        die("pipe() error");
    }
    pid_t subpid = fork();
    if (subpid < 0) {
        perror("fork in dns_output_start_subprocess()");
        die("fork() error");
    }
    if (subpid == 0) {

        // Subprocess
        // New process group - ignore sigint
        setpgid(0, 0);
        // Close a half of the pipe
        if (close(pipefds[1]) != 0) {
            perror("close in dns_output_start_subprocess() in subprocess");
            die("close() error");
        }
        // Connect pipe to stdin
        if (dup2(pipefds[0], 0) < 0) {
            perror("dup2 of input pipe in dns_output_start_subprocess() in subprocess");
            die("dup2() error");
        }
        // Connect output to stdout if defined
        fflush(stdout);
        if (outfile && strlen(outfile) > 0) {
            fclose(stdout);
            int outfd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY, 00644);
            if (outfd < 0) {
                perror("open output file in dns_output_start_subprocess() in subprocess");
                die("error opening \"%s\"", outfile);
            }
            if (dup2(outfd, 1) < 0) {
                perror("dup2 of open output file in dns_output_start_subprocess() in subprocess");
                die("dup2() error");
            }
        }
        // Close all fds except for (0, 1, 2)
        for (int fdi = 3; fdi < FOPEN_MAX; fdi++)
            close(fdi);
        fflush(stderr);
        execl("/bin/sh", "/bin/sh", "-c", sh_cmd, (char *)NULL);
        perror("exec in dns_output_start_subprocess() in subprocess");
        die("failed to exec: \"/bin/sh\" \"-c\" \"%s\"", sh_cmd);

    }
    // Master process
    if (close(pipefds[0]) != 0) {
        perror("close in dns_output_start_subprocess() in subprocess");
        die("close() error");
    }
    *pidp = subpid;
    return pipefds[1];
}


void
dns_output_open(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (!out->path) && (!out->out_file) && (time != DNS_NO_TIME));

    if (out->path_fmt && strlen(out->path_fmt) > 0) {
        // Extra space for expansion -- note that most used conversions are "in place": "%d" -> "01" 
        int path_len = strlen(out->path_fmt) + DNS_OUTPUT_FILENAME_EXTRA;
        out->path = xmalloc(path_len);
        size_t l = dns_us_time_strftime(out->path, path_len, out->path_fmt, time);
        if (l == 0)
            die("Expanded filename '%s' expansion too long.", out->path_fmt);
    } else {
        out->path = strdup("");
    }

    if (out->pipe_cmd && strlen(out->pipe_cmd) > 0) {
        // Run a subprocess
        out->out_fd = dns_output_start_subprocess(out->pipe_cmd, out->path, &(out->pipe_pid));
        out->out_file = fdopen(out->out_fd, "w");
        if (!out->out_file) {
            dns_output_wait_for_pipe_process(out, 0);
            die("Unable to open output pipe descriptor %d: %s.", out->out_fd, strerror(errno));
        }
    } else if (strlen(out->path) > 0) {
        // Open a file
        out->out_file = fopen(out->path, "w");
        if (!out->out_file)
            die("Unable to open output file '%s': %s.", out->path, strerror(errno));
        out->out_fd = fileno(out->out_file);
    } else {
        // Use stdout 
        out->out_fd = dup(1);
        if (out->out_fd < 0)
            die("Failed to dup() stdin");
        out->out_file = fdopen(out->out_fd, "w");
        if (!out->out_file)
            die("Unable to open dupped STDOUT at fd %d: %s", out->out_fd, strerror(errno));
    }

    out->output_opened = time;
    out->current_time = MAX(out->current_time, time);


    if (out->start_file)
          out->start_file(out, time);
}

void
dns_output_close(struct dns_output *out, dns_us_time_t time)
{
    assert(out && out->out_file && out->path && (time != DNS_NO_TIME));

    // Finish the format
    if (out->finish_file) {
        (out->finish_file)(out, time);
    }

    // Report and update time
    out->total_items += out->current_items;
    out->total_request_only += out->current_request_only;
    out->total_response_only += out->current_response_only;
    out->total_bytes += out->current_bytes;
    out->current_time = MAX(out->current_time, time);

    double rate_items = (out->current_items) / dns_us_time_to_fsec(out->current_time - out->output_opened);
    double rate_bytes = (out->current_bytes) / dns_us_time_to_fsec(out->current_time - out->output_opened);

    msg(L_INFO, "output written to \"%s\": %"PRIu64" (%.3lg/s) bytes (before pipe cmd)",
        strlen(out->path) > 0 ? out->path : "<STDOUT>", out->current_bytes, rate_bytes);
    msg(L_INFO, "output items: %"PRIu64" (%.3lg/s, %"PRIu64" req-only, %"PRIu64" resp-only)",
        out->current_items, rate_items, out->current_request_only, out->current_response_only);
    msg(L_INFO, "output totals: %"PRIu64" items (%"PRIu64" req-only, %"PRIu64" resp-only), %"PRIu64" bytes",
        out->total_items, out->total_request_only, out->total_response_only, out->total_bytes);

    out->current_items = 0;
    out->current_request_only = 0;
    out->current_response_only = 0;
    out->current_bytes = 0;

    // Close out file (with the fd)
    fclose(out->out_file);
    out->out_file = NULL;
    out->out_fd = -1;
    free(out->path);
    out->path = NULL;

    // Wait for the pipe process
    if (dns_output_wait_for_pipe_process(out, 1) == 2) {
        die("Pipe subprocess terminated with error, check your configuration.");
    }
}

/**
 * Check and potentionally rotate output files.
 * Opens the output if not open. Rotates output file after `out->period` time
 * has passed since the opening. Requires `time!=DNS_NO_TIME`.
 */
static void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (time != DNS_NO_TIME));

    // check if we need to switch output files
    if ((out->out_file) && (out->period_sec > 0) &&
        dns_next_rotation(out->period_sec, out->output_opened, time))
        dns_output_close(out, time);

    // open if not open
    if (!out->out_file)
        dns_output_open(out, time);

    out->current_time = MAX(out->current_time, time);
}

static void *
dns_output_main(void *data)
{
    struct dns_output *out = (struct dns_output *) data;

    int run = 1;
    while(run) {
        struct dns_packet_frame *f = dns_frame_queue_dequeue(out->in);
        if (f->type == 1) {
            assert(f->count == 0);
            run = 0;
        } else if (f->time_start == DNS_NO_TIME) {
            assert(f->count == 0);
        } else { // Regular frame
            if (dns_output_wait_for_pipe_process(out, 0) != 0) {
                die("Pipe subprocess terminated with error, check your configuration.");
            }
            dns_output_check_rotation(out, f->time_start);
            CLIST_FOR_EACH(struct dns_packet *, pkt, f->packets) {
                dns_output_check_rotation(out, pkt->ts);
                if (out->write_packet)
                    (out->write_packet)(out, pkt);
            }
        }
        dns_packet_frame_destroy(f);
    }
    if (out->out_file) {
        dns_output_close(out, out->current_time);
    }

    pthread_mutex_unlock(&out->running);
    return NULL;
}

void
dns_output_start(struct dns_output *out)
{
    if (pthread_mutex_trylock(&out->running) != 0)
        die("starting a running output thread");
    int r = pthread_create(&out->thread, NULL, dns_output_main, out);
    assert(r == 0);
    msg(L_DEBUG, "Output thread started");
}

void
dns_output_finish(struct dns_output *out)
{
    int r = pthread_join(out->thread, NULL);
    assert(r == 0);
    msg(L_DEBUG, "Output stopped and joined");
}

