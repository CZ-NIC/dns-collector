#include <assert.h>
#include <execinfo.h>
#include <signal.h>

#include "common.h"
#include "timeframe.h"
#include "collector.h"
#include "dns.h"
#include "output.h"

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
        OPT_HELP("the config file. When pcap-files are given, they are processed"),
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
    struct dns_config conf;
//    signal(SIGSEGV, segv_handler);

    GARY_INIT(main_inputs, 0);
    cf_declare_rel_section("collector", &dns_config_section, &conf, 0);

    log_register_type("spam");
    opt_parse(&dns_options, argv + 1);
    log_configured("default-log");

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-align"
    #pragma GCC diagnostic ignored "-Wpedantic"
    *(GARY_PUSH(main_inputs)) = NULL;
    #pragma GCC diagnostic pop

    if ((*main_inputs == NULL) && clist_empty(&conf.inputs)) {
        msg(L_FATAL, "Provide at least one input capture filename or configure capture device in the config.");
        return 1;
    }

    dns_collector_t *col = dns_collector_create(&conf);

    dns_collector_start_output_threads(col);

    if (*main_inputs) {
        // offline pcaps
        for (char **in = main_inputs; *in; in++)
            dns_collector_run_on_pcap(col, *in);
    } else {
        // live
        dns_collector_run_on_inputs(col, &conf.inputs, 0);
    }

    dns_collector_finish(col);
    dns_collector_stop_output_threads(col, dns_os_queue);

    //dns_stats_fprint(&(col->stats), col->conf, stderr);

    dns_collector_destroy(col);

    GARY_FREE(main_inputs);
    // Valgrind registers 13 unfreed blocks amounting to libUCW config data.
    // There seems to be no way to free it.

    return 0;
}
