#include <assert.h>

#include <ucw/lib.h>
#include <ucw/opt.h>

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
    struct dns_collector_config conf;
    GARY_INIT(main_inputs, 0);
    cf_declare_rel_section("collector", &dns_collector_config_section, &conf, 0);

    opt_parse(&dns_options, argv + 1);

    conf.inputs = main_inputs;
    dns_collector_t *col = dns_collector_create(&conf);

    collector_run(col);

    dns_stats_fprint(&(col->stats), col->config, stderr);
    dns_collector_destroy(col);
    // TODO: free the input strings
    GARY_FREE(main_inputs);

    return 0;
}
