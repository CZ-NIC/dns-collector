#include <assert.h>

#include "common.h"
#include "timeframe.h"
#include "collector.h"

int main(int argc, const char **argv)
{
    dns_ret_t r;

    assert(argc > 1);

    dns_collector_config_t conf = {"out", 10, {10, 0}, 65535, {1, 1, 1, 1, 1, 1, 1, 1, 1}}; 

    dns_collector_t *col = dns_collector_create(&conf);
    assert(col);

    r = dns_collector_dump_open(col, "dump.pcap");
    assert(r == DNS_RET_OK);

    for (int in = 1; in < argc; in++) {
        r = dns_collector_open_pcap_file(col, argv[in]);
        assert(r == DNS_RET_OK);
        
        while(r = dns_collector_next_packet(col), r == DNS_RET_OK) { }
        if (r == DNS_RET_ERR)
            break;
    }

    dns_stats_fprint(&(col->stats), col->config, stderr);
    dns_collector_destroy(col);

    return 0;
}
