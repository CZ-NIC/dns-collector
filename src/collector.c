
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>

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


static dns_us_time_t
dns_trace_packet_time(libtrace_packet_t *tp)
{
    struct timeval now_tv = trace_get_timeval(tp);
    return dns_us_time_from_timeval(&now_tv);
}


void
dns_collector_update_rotation(dns_collector_t *col, dns_us_time_t time)
{
    if (!col->tf_cur) {
         dns_collector_rotate_frames(col, time);
    }

    // Possibly rotate several times to fill any gaps
    while (col->tf_cur->time_start + col->conf->timeframe_length < time) {
        dns_collector_rotate_frames(col, col->tf_cur->time_start + col->conf->timeframe_length);
    }
}


void
dns_collector_run_on_pcap(dns_collector_t *col, char *inuri)
{
    assert(col && inuri);

    struct dns_input input = {
        .uri = inuri,
        .snaplen = -1,
        .promisc = 0,
        .bpf_string = NULL, // TODO: allow filtering for pcaps as well?
        .bpf_filter = NULL,
        .offline = 1,
    };

    clist inputs;
    clist_init(&inputs);
    clist_add_head(&inputs, &input.n);

    col->conf->wait_for_outputs = 1;
    dns_collector_run_on_inputs(col, &inputs, 1);
}

void
dns_collector_run_on_inputs(dns_collector_t *col, clist *inputs, int offline)
{
    assert(col);

    CLIST_FOR_EACH(struct dns_input*, input, *inputs) {
        assert(input->uri);
        if (dns_input_open(input) != DNS_RET_OK)
            die("could not open '%s'", input->uri);
        assert(input->trace && input->packet);
        if ((! ! offline) != (! ! input->offline))
            die("Live and offline inputs may not be combined.");
    }

    CLIST_FOR_EACH(struct dns_input*, input, *inputs) {
        if(trace_start(input->trace) < 0) {
            msg(L_FATAL, "libtrace error starting '%s': %s", input->uri, trace_get_err(input->trace).problem);
            die("error starting capture");
        }
    }

    libtrace_eventobj_t ev;
    int stop = 0;
    fd_set fd_in;

    while (!stop) {
        // Wake up at least twice per timeframe
        double sleep = dns_us_time_to_fsec(col->conf->timeframe_length) / 2;
        int someread = 0;
        int largest_fd = -1;
        FD_ZERO(&fd_in);

        CLIST_FOR_EACH(struct dns_input*, input, *inputs) {
            // rotate frames and inputs if online
            if (!offline) {
                struct timespec tp;
                if (clock_gettime(CLOCK_REALTIME, &tp) < 0)
                    die("clock_gettime(CLOCK_REALTIME, ...) error");
                dns_collector_update_rotation(col, dns_us_time_from_timespec(&tp));
            }

            // receive the next event
            ev = trace_event(input->trace, input->packet);
            switch (ev.type) {
            case TRACE_EVENT_PACKET:
                dns_collector_update_rotation(col, dns_trace_packet_time(input->packet));
                dns_collector_process_packet(col, input->packet);
                someread = 1;
                break;
            case TRACE_EVENT_TERMINATE:
                msg(L_DEBUG, "finished reading '%s'", input->uri);
                stop = 1;
                break;
            case TRACE_EVENT_SLEEP:
                sleep = MIN(ev.seconds, sleep);
                break;
            case TRACE_EVENT_IOWAIT:
                FD_SET(ev.fd, &fd_in);
                largest_fd = MAX(ev.fd, largest_fd);
                break;
            default:
                assert(!"Unknown event");
            }

            if (stop)
                break;
        }

        if (stop)
            break;

        // all inputs blocking - wait or select
        if (!someread) {
            dns_us_time_t us = sleep * 1000000;
            if (largest_fd >= 0) {
                struct timeval tv = {.tv_sec = us / 1000000, .tv_usec = us % 1000000 };
                int r = select( largest_fd + 1, &fd_in, NULL, NULL, &tv );
                if (r < 0)
                    die("select error: %s", strerror(errno));
            } else {
                usleep((useconds_t)(sleep * 1000000));
            }
        }
    }

    CLIST_FOR_EACH(struct dns_input*, input, *inputs) {
        dns_input_close(input);
        assert(!input->trace);
    }

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
    assert(col && (!col->tf_old) && (!col->tf_cur));

    CLIST_FOR_EACH(struct dns_output*, out, col->conf->outputs) {
        dns_output_destroy(out);
    }
 
    pthread_mutex_destroy(&col->collector_mutex);
    pthread_cond_destroy(&col->output_cond);
    pthread_cond_destroy(&col->unblock_cond);

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

