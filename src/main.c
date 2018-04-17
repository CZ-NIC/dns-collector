/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "frame_queue.h"
#include "input.h"
#include "output_csv.h"
#include "output_cbor.h"
#include "packet_frame.h"
#include "worker_frame_logger.h"
#include "worker_packet_matcher.h"

#define MAX_TRACE_SIZE 42
static void
sigsegv_handler(int sig UNUSED)
{
    void *array[MAX_TRACE_SIZE];
    size_t size = backtrace(array, MAX_TRACE_SIZE);
    msg(L_WARN | L_SIGHANDLER, "Received SEGV, printing trace and then running default SEGV action.");
    backtrace_symbols_fd(array, size, 2);
    signal(SIGSEGV, SIG_DFL);
    kill(getpid(), SIGSEGV);
}

static void
sigint_handler(int sig UNUSED)
{
    if (dns_global_stop) {
        msg(L_WARN | L_SIGHANDLER, "Received another SIGINT, exitting immediatelly");
        exit(1);
    } else {
        dns_global_stop = 1;
        msg(L_INFO | L_SIGHANDLER, "Received SIGINT: stopping input, graceful shutdown (send again to kill immediatelly)");
    }
}

static void
sigpipe_handler(int sig UNUSED)
{
    if (dns_global_stop) {
        msg(L_WARN | L_SIGHANDLER | DNS_MSG_SPAM, "Ignoring SIGPIPE after input termination (likely minor error)");
    } else {
        msg(L_FATAL | L_SIGHANDLER, "Received SIGPIPE during operation, exitting.");
        exit(1);
    }
}

static void UNUSED
signal_ignore_handler(int sig)
{
    msg(L_INFO | L_SIGHANDLER, "Ignoring signal %s (%d)", sys_siglist[sig], sig);
}

static char **main_inputs; // growing array of char*
static char *main_interface = NULL; // pointer to static string
static char *main_output_path = NULL; // pointer to static string

static struct opt_section dns_options = {
    OPT_ITEMS {
        OPT_HELP("A collector of DNS queries."),
        OPT_HELP("Usage: main [options] [pcap-files...]"),
        OPT_HELP(""),
        OPT_HELP("When no pcap-files are given or with '-i', a live trace is run based on"),
        OPT_HELP("the config file. When pcap files are given, they are processed"),
        OPT_HELP("offline in the given order, ignoring any configured input uri."),
        OPT_HELP(""),
        OPT_HELP("Options:"),
        OPT_HELP_OPTION,
        OPT_CONF_OPTIONS,
        OPT_STRING('i', "interface", main_interface, OPT_REQUIRED_VALUE, "Runs a live capture on an interface, has the same semantics as 'input_uri'."),
        OPT_STRING('o', "output", main_output_path, OPT_REQUIRED_VALUE, "Output filename pattern, has the same semantics as 'output_path_fmt'."),
        OPT_STRING_MULTIPLE(OPT_POSITIONAL_TAIL, NULL, main_inputs, OPT_BEFORE_CONFIG, ""),
        OPT_END
    }
};


int main(int argc UNUSED, char **argv)
{
    dns_ret_t r;

    // Setup signal handlers

    signal(SIGSEGV, sigsegv_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);

    // Configure
    struct dns_config *conf = alloca(sizeof(struct dns_config));

    GARY_INIT(main_inputs, 0);
    dns_log_spam_type = log_register_type("spam");
    cf_declare_rel_section("dnscol", &dns_config_section, conf, 0);
    // Pre-configure the logging system
    cf_journal_new_transaction(1);
    opt_parse(&dns_options, argv + 1);

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-align"
    #pragma GCC diagnostic ignored "-Wpedantic"
    *(GARY_PUSH(main_inputs)) = NULL;
    #pragma GCC diagnostic pop

    if (main_interface != NULL) {
        conf->input_uri = strdup(main_interface);
        if (*main_inputs != NULL) {
            opt_failure("ERROR: Both input interface (-i) and input pcap files provided.");
            return 2;
        }
    }

    if (main_output_path != NULL) {
        conf->output_path_fmt = strdup(main_output_path);
    }

    if (conf->output_path_fmt == NULL || strlen(conf->output_path_fmt) == 0) {
        die("Configure the output with '-o' or config 'output_path_fmt'.");
    }

    if ((*main_inputs == NULL) && strlen(conf->input_uri) == 0) {
        opt_failure("ERROR: Provide at least one of: input pcap filename, interface name (with '-i') or configure 'input_uri'.");
        return 2;
    }
    if(log_check_configured("stderr") != NULL)
        cf_set("logging { stream { name stderr; filedesc 2; types:reset default; "
            "levels:reset error fatal info warn; } }");
    if(log_check_configured("default") != NULL)
        cf_set("logging { stream { name default; substream stderr; } }");
    log_configured("default");
    log_set_format(log_stream_by_flags(0), 0, LSFMT_LEVEL | LSFMT_TIME | LSFMT_TITLE | LSFMT_PID | LSFMT_USEC | LSFMT_TYPE);

    // Construct and start workflow

    struct dns_frame_queue *q_input_mathcher =
        dns_frame_queue_create(conf->max_queue_len, DNS_QUEUE_BLOCK);
    struct dns_frame_queue *q_matcher_output =
        dns_frame_queue_create(conf->max_queue_len, DNS_QUEUE_BLOCK);
    struct dns_input *input =
        dns_input_create(conf, q_input_mathcher);
    struct dns_worker_packet_matcher *w_matcher =
        dns_worker_packet_matcher_create(conf, q_input_mathcher, q_matcher_output);

    struct dns_output *output;
    switch (conf->output_type) {
        case DNS_OUTPUT_TYPE_CSV:
            output = (struct dns_output *)dns_output_csv_create(conf, q_matcher_output);
            break;
        case DNS_OUTPUT_TYPE_CBOR:
            output = (struct dns_output *)dns_output_cbor_create(conf, q_matcher_output);
            break;
        default: die("Invalid output type");
    }

    dns_worker_packet_matcher_start(w_matcher);
    output->start_output(output);

    // Main loop, start inputs

    if (*main_inputs) {
        // offline pcaps
        for (char **in = main_inputs; *in; in++) {
            char fn[1024];
            FILE *f = fopen(*in, "rb");
            if (f == NULL) {
                msg(L_ERROR, "Error opening file '%s': %s", *in, strerror(errno));
                if (in == main_inputs) {
                    die("No output written.");
                }
                break;
            }
            fclose(f);
            snprintf(fn, sizeof(fn), "pcapfile:%s", *in);
            r = dns_input_process(input, fn);
            if (r != DNS_RET_OK) {
                msg(L_ERROR, "Processing of '%s' unsuccesfull (code %d)", fn, r);
            }
        }
    } else {
        // live
        input->online = 1;
        r = dns_input_process(input, NULL);
        if (r != DNS_RET_OK) {
            msg(L_ERROR, "Processing of '%s' unsuccesfull (code %d)", input->uri, r);
        }
    }

    // Send the last frame, wait for threads to exit

    dns_input_finish(input);
    dns_worker_packet_matcher_finish(w_matcher);
    output->finish_output(output);

    // Dealloc and cleanup

    dns_input_destroy(input);
    dns_worker_packet_matcher_destroy(w_matcher);
    output->finalize_output(output);
    free(output);
    dns_frame_queue_destroy(q_input_mathcher);
    dns_frame_queue_destroy(q_matcher_output);

    GARY_FREE(main_inputs);

    // NOTE: Valgrind registers 13 unfreed blocks amounting to libUCW config data.
    // There seems to be no clean way to free it.

    return 0;
}
