#include <assert.h>

#include "common.h"
#include "timeframe.h"
#include "collector.h"
#include "dns.h"
#include "output.h"


static char **main_inputs; // growing array of char*
static struct opt_section dns_options = {
    OPT_ITEMS {
        OPT_HELP("A collector of DNS queries."),
        OPT_HELP("Usage: main [options] pcap-files..."),
        OPT_HELP(""),
        OPT_HELP("Options:"),
        OPT_HELP_OPTION,
        OPT_CONF_OPTIONS,
        OPT_STRING_MULTIPLE(OPT_POSITIONAL_TAIL, NULL, main_inputs, OPT_REQUIRED | OPT_BEFORE_CONFIG, ""),
        OPT_END
    }
};


int main(int argc UNUSED, char **argv)
{
    struct dns_config conf;

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
    conf.inputs = main_inputs;

    dns_collector_t *col = dns_collector_create(&conf);

    collector_run(col);

    dns_stats_fprint(&(col->stats), col->config, stderr);

    dns_collector_destroy(col);

    GARY_FREE(main_inputs);
    // Valgrind registers 13 unfreed blocks amounting to libUCW config data.
    // There seems to be no way to free it.

    return 0;
}
