
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libtrace.h>

#include "collector.h"
#include "timeframe.h"
#include "packet.h"

dns_collector_t *
dns_collector_create(struct dns_config *conf)
{
    assert(conf);

    dns_collector_t *col = (dns_collector_t *)xmalloc_zero(sizeof(dns_collector_t));

    pthread_mutex_init(&(col->collector_mutex), NULL);
    pthread_cond_init(&(col->output_cond), NULL);
    pthread_cond_init(&(col->unblock_cond), NULL);
  
    col->conf = conf;
    CLIST_FOR_EACH(struct dns_output*, out, conf->outputs) {
        msg(L_DEBUG, "Collector configuring output '%s'", out->path_fmt);
        dns_output_init(out, col);
    }

    col->packet = trace_create_packet();
    if (!col->packet)
        die("FATAL: libtrace packet allocation error!");
    
    return col;
}

void
dns_collector_start_output_threads(dns_collector_t *col)
{
    pthread_mutex_lock(&(col->collector_mutex));
    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        msg(L_DEBUG, "Creating thread for %s", out->path_fmt);
        int r = pthread_create(&(out->thread), NULL, dns_output_thread_main, out);
        if (r)
            die("Thread creation failed [err %d].", r);
    }
    pthread_mutex_unlock(&(col->collector_mutex));
}

void
dns_collector_stop_output_threads(dns_collector_t *col, enum dns_output_stop how)
{
    assert(col && how != dns_os_none);

    pthread_mutex_lock(&(col->collector_mutex));
    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        out->stop_flag = how;
    }
    pthread_cond_broadcast(&(col->output_cond));
    pthread_mutex_unlock(&col->collector_mutex);

    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        pthread_join(out->thread, NULL);
    }
    // TODO(tomas): detect pthread errors
}

void
dns_collector_run_on_input(dns_collector_t *col, char *inuri)
{
    assert(col && (inuri || col->conf->input.uri) && (!col->trace) && col->packet);

    if (dns_trace_open(col, inuri) != DNS_RET_OK)
        return;
    assert(col->trace);

    int r = trace_start(col->trace);
    if (r < 0) {
        msg(L_FATAL, "libtrace error starting '%s': %s", inuri, trace_get_err(col->trace).problem);
        trace_destroy(col->trace);
        col->trace = NULL;
        return;
    }

    while (1) {
        r = trace_read_packet(col->trace, col->packet);
        if (r < 0) {
            msg(L_FATAL, "libtrace error reading '%s': %s", inuri, trace_get_err(col->trace).problem);
            break;
        }
        if (r == 0) {
            msg(L_DEBUG, "finished reading '%s'", inuri);
            break;
        }

        col->stats.packets_captured++;

        struct timeval now_tv = trace_get_timeval(col->packet);
        dns_us_time_t now = dns_us_time_from_timeval(&now_tv);
        if (!col->tf_cur) {
             dns_collector_rotate_frames(col, now);
        }

        // Possibly rotate several times to fill any gaps
        while (col->tf_cur->time_start + col->conf->timeframe_length < now) {
            dns_collector_rotate_frames(col, col->tf_cur->time_start + col->conf->timeframe_length);
        }

        dns_collector_process_packet(col, col->packet);
    }

    dns_trace_close(col);
}

void
dns_collector_output_timeframe(struct dns_collector *col, struct dns_timeframe *tf)
{
    assert(col && tf && (tf->refcount == 0));

    pthread_mutex_lock(&(col->collector_mutex));

    msg(L_DEBUG, "Pushing frame %.2f - %.2f (%d queries) to all outputs",
        dns_us_time_to_fsec(tf->time_start), dns_us_time_to_fsec(tf->time_end),
        tf->packets_count);
    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        if (col->conf->wait_for_outputs) {
            // wait for output to be ready
            while (dns_output_queue_space(out) <= 0) {
                pthread_cond_wait(&col->unblock_cond, &col->collector_mutex);
            }
        }
        // drop a frame when queue full
        dns_output_push_frame(out, tf);
    }
    // inc and dec refcount to free in case frame was not inserted anywhere
    dns_timeframe_incref(tf);
    dns_timeframe_decref(tf);

    pthread_cond_broadcast(&(col->output_cond));
    pthread_mutex_unlock(&(col->collector_mutex));
}

void
dns_collector_finish(dns_collector_t *col)
{ 
    assert(col);

    if (col->tf_old) {
        dns_collector_output_timeframe(col, col->tf_old);
        col->tf_old = NULL;
    }

    if (col->tf_cur) {
        dns_collector_output_timeframe(col, col->tf_cur);
        col->tf_cur = NULL;
    }
}

void
dns_collector_destroy(dns_collector_t *col)
{ 
    assert(col && (!col->tf_old) && (!col->tf_cur) && col->packet && (!col->trace));

    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        dns_output_destroy(out);
    }
 
    pthread_mutex_destroy(&col->collector_mutex);
    pthread_cond_destroy(&col->output_cond);
    pthread_cond_destroy(&col->unblock_cond);

    trace_destroy_packet(col->packet);

    free(col);
}


void
dns_collector_process_packet(dns_collector_t *col, libtrace_packet_t *tp)
{
    assert(col && col->tf_cur && tp);

    dns_parse_error_t err;
    dns_packet_t *pkt = dns_packet_create_from_libtrace(col, tp, &err);
    if (!pkt) {
        // TODO(tomas): drop packet
        msg(L_WARN, "Dropping packet, err: %d", err);
        return;
    }

    // Matching request 
    dns_packet_t *req = NULL;

    if (DNS_PACKET_IS_RESPONSE(pkt)) {
        if (col->tf_old) {
            req = dns_timeframe_match_response(col->tf_old, pkt);
        }
        if (req == NULL) {
            req = dns_timeframe_match_response(col->tf_cur, pkt);
        }
    }

    if (req)
        return; // packet given to a matching request
        
    dns_timeframe_append_packet(col->tf_cur, pkt);
}


void
dns_collector_rotate_frames(dns_collector_t *col, dns_us_time_t time_now)
{
    assert(col);

    if (col->tf_old) {
        dns_collector_output_timeframe(col, col->tf_old);
        col->tf_old = NULL;
    }

    if (time_now == DNS_NO_TIME) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        time_now = dns_us_time_from_timespec(&now);
    }

    if (col->tf_cur) {
        col->tf_cur -> time_end = time_now - 1; // prevent overlaps
        col->tf_old = col->tf_cur;
        col->tf_cur = NULL;
    }

    col->tf_cur = dns_timeframe_create(col->conf, time_now);
}

