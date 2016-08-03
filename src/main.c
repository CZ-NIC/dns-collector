#include <assert.h>
#include <execinfo.h>
#include <signal.h>

#include "common.h"
#include "packet_frame.h"
#include "worker_frame_logger.h"
#include "worker_packet_matcher.h"
#include "input.h"
#include "frame_queue.h"
//#include "collector.h"
//#include "dns.h"
//#include "output.h"

#define MAX_TRACE_SIZE 42
void segv_handler(int sig UNUSED) {
    void *array[MAX_TRACE_SIZE];
    size_t size = backtrace(array, MAX_TRACE_SIZE);
    backtrace_symbols_fd(array, size, 2);
    exit(1);
}

static char **main_inputs; // growing array of char*

static struct opt_section dns_options = {
    OPT_ITEMS {
        OPT_HELP("A collector of DNS queries."),
        OPT_HELP("Usage: main [options] [pcap-files...]"),
        OPT_HELP(""),
        OPT_HELP("When no pcap-files are given, a live trace is run based on"),
        OPT_HELP("the config file. When pcap files are given, they are processed"),
        OPT_HELP("in sequence, ignoring any configured input."),
        OPT_HELP(""),
        OPT_HELP("Options:"),
        OPT_HELP_OPTION,
        OPT_CONF_OPTIONS,
        OPT_STRING_MULTIPLE(OPT_POSITIONAL_TAIL, NULL, main_inputs, OPT_BEFORE_CONFIG, ""),
        OPT_END
    }
};


int main(int argc UNUSED, char **argv)
{
    dns_ret_t r;

    // Setup segv handler

    signal(SIGSEGV, segv_handler);

    // Construct workflow

    struct dns_frame_queue *q_in_log1 = dns_frame_queue_create(5, 0, DNS_QUEUE_BLOCK);
    struct dns_frame_queue *q_log1_match = dns_frame_queue_create(5, 0, DNS_QUEUE_BLOCK);
    struct dns_frame_queue *q_match_log2 = dns_frame_queue_create(5, 0, DNS_QUEUE_BLOCK);
    struct dns_frame_queue *q_log2_out = NULL;

    struct dns_input *input = dns_input_create(q_in_log1);
    struct dns_worker_frame_logger *w_log1 = dns_worker_frame_logger_create("log1", q_in_log1, q_log1_match);
    struct dns_worker_packet_matcher *w_match = dns_worker_packet_matcher_create(dns_fsec_to_us_time(20.0), q_log1_match, q_match_log2);
    struct dns_worker_frame_logger *w_log2 = dns_worker_frame_logger_create("log2", q_match_log2, q_log2_out);

    dns_worker_frame_logger_start(w_log1);
    dns_worker_frame_logger_start(w_log2);
    dns_worker_packet_matcher_start(w_match);

    // Configure

    GARY_INIT(main_inputs, 0);
    cf_declare_rel_section("dnscol_input", &dns_input_section, input, 0);
    log_register_type("spam");
    opt_parse(&dns_options, argv + 1);

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-align"
    #pragma GCC diagnostic ignored "-Wpedantic"
    *(GARY_PUSH(main_inputs)) = NULL;
    #pragma GCC diagnostic pop

    if ((*main_inputs == NULL) && strlen(input->uri) == 0) {
        opt_failure("Provide at least one input capture filename or configure capture device in the config.");
        return 2;
    }

    log_configured("default-log");
    log_set_format(log_stream_by_flags(0), 0, LSFMT_LEVEL | LSFMT_TIME | LSFMT_TITLE | LSFMT_PID | LSFMT_USEC );

    // Main loop

    if (*main_inputs) {
        // offline pcaps
        for (char **in = main_inputs; *in; in++) {
            char fn[1024];
            snprintf(fn, sizeof(fn), "pcapfile:%s", *in);
            r = dns_input_process(input, fn);
            if (r != DNS_RET_OK) {
                msg(L_INFO, "Processing of %s unsuccesfull (code %d)", fn, r);
            }
        }
    } else {
        // live
        input->online = 1;
        r = dns_input_process(input, NULL);
        if (r != DNS_RET_OK) {
            msg(L_INFO, "Processing of %s unsuccesfull (code %d)", input->uri, r);
        }
    }

    // Send the last frame, wait for threads to exit

    dns_input_finish(input);
    dns_worker_frame_logger_finish(w_log1);
    dns_worker_frame_logger_finish(w_log2);
    dns_worker_packet_matcher_finish(w_match);

    // Dealloc and cleanup

    dns_input_destroy(input);
    dns_worker_frame_logger_destroy(w_log1);
    dns_worker_frame_logger_destroy(w_log2);
    dns_worker_packet_matcher_destroy(w_match);
    dns_frame_queue_destroy(q_in_log1);
    dns_frame_queue_destroy(q_log1_match);
    dns_frame_queue_destroy(q_match_log2);
//    dns_frame_queue_destroy(q_log2_out);
    GARY_FREE(main_inputs);

    // NOTE: Valgrind registers 13 unfreed blocks amounting to libUCW config data.
    // There seems to be no clean way to free it.

    return 0;
}
