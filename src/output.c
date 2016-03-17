#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>
#include <lz4.h>
#include <lz4frame.h>

#include "common.h"
#include "collector.h"
#include "timeframe.h"
#include "output.h"
#include "output_compression.h"

char *
dns_output_conf_init(struct dns_output *out)
{
    out->period_sec = 300.0; // 5 min

    out->compression = dns_oc_none;

    return NULL;
}


char *
dns_output_conf_commit(struct dns_output *out)
{
    if (out->period_sec < 0.000001)
        out->period = 0;
    else
        out->period = dns_fsec_to_us_time(out->period_sec);

    if (!out->path_fmt)
        return "'path' needed in output.";

    if (out->compression >= dns_oc_LAST)
        return "Invalid compression.";

    return NULL;
}


void
dns_output_init(struct dns_output *out, struct dns_collector *col)
{
    assert(out && col && (!out->queue));

    out->collector_output_cond = &(col->output_cond);
    out->collector_unblock_cond = &(col->unblock_cond);
    out->collector_mutex = &(col->collector_mutex);

    out->max_queue_len = col->conf->max_queue_len;
    out->queue = (struct dns_timeframe **) xmalloc_zero(sizeof(struct dns_timeframe *) * out->max_queue_len);
    out->output_opened = DNS_NO_TIME;
}


void
dns_output_destroy(struct dns_output *out)
{
    assert(out && out->queue);

    struct dns_timeframe *frame;
    while((frame = dns_output_pop_frame(out))) {
        msg(L_WARN, "Leftover frame on dns_output_destroy() of output %s", out->path_fmt);
        dns_timeframe_decref(frame);
    }

    xfree(out->queue);
    out->queue = NULL;
    out->max_queue_len = 0;
}

void *
dns_output_thread_main(void *data)
{
    struct dns_output *out = (struct dns_output *) data;
    struct dns_timeframe *current_frame = NULL;
    dns_us_time_t last_frame_end = DNS_NO_TIME;

    // acquire the collector mutex
    pthread_mutex_lock(out->collector_mutex);
    while(1) {

        // check stop conditions
        if ((out->stop_flag == dns_os_frame) ||
            ((out->stop_flag == dns_os_queue) && (out->queue_len == 0))) {
            break;
        }

        // acquire a frame or wait for it
        assert(!current_frame);
        current_frame = dns_output_pop_frame(out);
        if (current_frame) {
            // unlock the mutex
            pthread_mutex_unlock(out->collector_mutex);

            // process the output with collector mutex unlocked
            dns_output_check_rotation(out, current_frame->time_start);
            last_frame_end = current_frame->time_end;
            dns_output_write_frame(out, current_frame);

            // relock the mutex, decref the frame
            pthread_mutex_lock(out->collector_mutex);

            dns_timeframe_decref(current_frame);
            current_frame = NULL;

            // Signal the collector that a frame was removed from the queue.
            pthread_cond_broadcast(out->collector_unblock_cond);
        } else {
            // unlock the mutex and wait for wakeup condition, then repeat
            pthread_cond_wait(out->collector_output_cond, out->collector_mutex);
        }
    }
    pthread_mutex_unlock(out->collector_mutex);
    dns_output_close(out, last_frame_end);

    return NULL;
}


struct dns_timeframe *
dns_output_pop_frame(struct dns_output *out)
{
    if (out->queue_len == 0)
        return NULL;

    struct dns_timeframe *next = out->queue[0];
    memcpy(out->queue + 0, out->queue + 1, sizeof(struct dns_timeframe *) * (out->queue_len - 1));
    out->queue[out->queue_len - 1] = NULL;
    out->queue_len --;
    return next;
}

int
dns_output_queue_space(struct dns_output *out)
{
    return out->max_queue_len - out->queue_len;
}

void
dns_output_push_frame(struct dns_output *out, struct dns_timeframe *tf)
{
    assert(out && tf && (out->max_queue_len >= 2) &&
           (out->queue_len <= out->max_queue_len));

    if (dns_output_queue_space(out) <= 0) {
        // Queue full: drop the oldest frame from the queue.
        struct dns_timeframe *drop = dns_output_pop_frame(out);
        msg(L_WARN, "Dropping frame %.2f - %.2f (%d queries) at output %s",
            dns_us_time_to_fsec(drop->time_start), dns_us_time_to_fsec(drop->time_end),
            drop->packets_count, out->path_fmt);
        dns_timeframe_decref(drop);
    }

    out->queue[out->queue_len] = tf;
    out->queue_len ++;
    dns_timeframe_incref(tf);
}

void
dns_output_write_frame(struct dns_output *out, struct dns_timeframe *tf)
{
    assert(out && tf);
    struct dns_packet *pkt = tf->packets;

    if (out->write_packet) {
        while(pkt) {
            out->write_packet(out, pkt);
            pkt = pkt -> next_in_timeframe;
        }
        /*
        msg(L_DEBUG, "Wrote frame %.2f - %.2f (%d queries) to output %s",
            dns_us_time_to_fsec(tf->time_start), dns_us_time_to_fsec(tf->time_end),
            tf->packets_count, out->path_fmt);
        */
    }
}


#define DNS_OUTPUT_FILENAME_STRFTIME_EXTRA 64

void
dns_output_open_file(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (!out->f) && (!out->path) && (time != DNS_NO_TIME));

    // Extra space for expansion -- note that most used conversions are "in place": "%d" -> "01" 
    int path_len = strlen(out->path_fmt) + DNS_OUTPUT_FILENAME_STRFTIME_EXTRA;
    out->path = xmalloc(path_len);
    size_t l = dns_us_time_strftime(out->path, path_len, out->path_fmt, time);

    if (l == 0)
        die("Expanded filename '%s' expansion too long.", out->path_fmt);
        
    out->f = fopen(out->path, "w");
    if (!out->f)
        die("Unable to open output file '%s': %s.", out->path, strerror(errno));
}


void
dns_output_close_file(struct dns_output *out, dns_us_time_t time UNUSED)
{
    assert(out && out->f && out->path);

    fclose(out->f);
    out->f = NULL;
    xfree(out->path);
    out->path = NULL;
}


void
dns_output_open(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (time != DNS_NO_TIME));

    out->wrote_bytes = out->wrote_bytes_compressed = out->wrote_items = 0;

    if (out->open_file)
        out->open_file(out, time);
    else
        dns_output_open_file(out, time);

    out->output_opened = time;

    dns_output_start_compression(out);

    if (out->start_file)
          out->start_file(out, time);
}


void
dns_output_close(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (time != DNS_NO_TIME));

    if (out->output_opened == DNS_NO_TIME)
        return;

    if (out->finish_file)
          out->finish_file(out, time);

    dns_output_finish_compression(out);

    if (out->compression != dns_oc_none)
        msg(L_INFO, "Output %lu B [%.1f B/q] compressed to %lu B [%.1f B/q] (%.1f%%), %lu items to '%s'",
            out->wrote_bytes, 1.0 * out->wrote_bytes / out->wrote_items,
            out->wrote_bytes_compressed, 1.0 * out->wrote_bytes_compressed / out->wrote_items, 100.0 * out->wrote_bytes_compressed / out->wrote_bytes,
            out->wrote_items, out->path);
    else
        msg(L_INFO, "Output %lu B [%.1f B/q], %lu items to '%s'",
            out->wrote_bytes, 1.0 * out->wrote_bytes / out->wrote_items,
            out->wrote_items, out->path);

    if (out->close_file)
          out->close_file(out, time);
    else
        dns_output_close_file(out, time);

    out->output_opened = DNS_NO_TIME;
}


void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (time != DNS_NO_TIME));

    // check if we need to switch output files
    if ((out->period > 0) && (out->output_opened != DNS_NO_TIME) &&
        (time >= out->output_opened + out->period - DNS_OUTPUT_ROTATION_GRACE_US))
        dns_output_close(out, time);

    // open if not open
    if (out->output_opened == DNS_NO_TIME)
        dns_output_open(out, time);
}

