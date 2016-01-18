#include <assert.h>

#include "common.h"
#include "timeframe.h"
#include "collector.h"

int main(int argc, const char **argv)
{
    assert(argc > 1);

    dns_collector_config_t conf = {"out", 10, {10, 0}, "out-except"}; 

    dns_collector_t *col = dns_collector_create(&conf);
    assert(col);

    int r;
    for (int in = 1; in < argc; in++) {
        r = dns_collector_open_pcap(col, argv[in]);
        assert(r == 0);
        
        while((r = dns_collector_next_packet(col)) >= 0) {
        }
        if (r == -1)
            break;
    }

    dns_collector_write_stats(col, stderr);
    dns_collector_destroy(col);
    return 0;
}
