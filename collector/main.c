#include <assert.h>

#include <ucw/lib.h>
#include <ucw/opt.h>

#include "common.h"
#include "timeframe.h"
#include "collector.h"
#include "dns.h"
#include "output.h"

static void add_input(const struct opt_item * opt UNUSED, const char * name, void * data UNUSED)
{
    // TODO
}

static struct opt_section dns_options = {
    OPT_ITEMS {
        OPT_HELP("A collector of DNS queries."),
        OPT_HELP("Usage: main [options] pcap-files..."),
        OPT_HELP(""),
        OPT_HELP("Options:"),
        OPT_HELP_OPTION,
        OPT_CONF_OPTIONS,
        OPT_CALL(OPT_POSITIONAL_TAIL, NULL, &add_input, NULL, OPT_REQUIRED | OPT_BEFORE_CONFIG, ""),
        OPT_END
    }
};


int main(int argc, char **argv)
{
    struct dns_collector_config conf;

    cf_declare_rel_section("collector", &dns_collector_config_section, &conf, 0);

    opt_parse(&dns_options, argv + 1);

    dns_collector_t *col = dns_collector_create(&conf);

    return 0; ////// TEST



    dns_ret_t r;
  
    r = dns_collector_dump_open(col, "dump.pcap");
    assert(r == DNS_RET_OK);

    for (int in = 1; in < argc; in++) {
        r = dns_collector_open_pcap_file(col, argv[in]);
        assert(r == DNS_RET_OK);
        
        while(r = dns_collector_next_packet(col), r == DNS_RET_OK) { }
        if (r == DNS_RET_ERR)
            break;
    }

    // Write tf_old
    dns_collector_rotate_frames(col, col->tf_cur->time_start + col->config->timeframe_length); // Hacky
    // Write tf_cur
    dns_collector_rotate_frames(col, 0); // Any time will do

    dns_stats_fprint(&(col->stats), col->config, stderr);
    dns_collector_destroy(col);

    return 0;
}
