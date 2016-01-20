
#include <assert.h>
#include <stdio.h>

#include "stats.h"

struct dns_drop_reason_desc { 
    dns_drop_reason reason;
    const char *name;
};

static const struct dns_drop_reason_desc dns_drop_reason_desc_table[] = {
    {dns_drop_malformed, "malformed"},
    {dns_drop_fragmented, "fragmented"},
    {dns_drop_tcp, "tcp"},
    {dns_drop_direction, "direction"},
    {dns_drop_port, "port"},
    {dns_drop_bad_dns, "bad_dns"},
    {dns_drop_frame_full, "frame_full"},
    {dns_drop_no_query, "no_query"},
    {dns_drop_other, "other"},
    {-1} // Guard: reason < 0
};



void
dns_stats_fprint(const dns_stats_t *stats, const dns_collector_config_t *conf, FILE *f)
{ 
    assert(stats && f);

    fprintf(f, "packets captured: \t%llu\n", stats->packets_captured);
    fprintf(f, "packets dropped and dumped:\n");
    for (const struct dns_drop_reason_desc *r = dns_drop_reason_desc_table; r->reason >= 0; r++) {
        int conf_to_dump = conf ? (conf->dump_packet_reason[r->reason]) : 0;
        fprintf(f, "\t%12s \t%10llu \t%10llu \t%s\n", r->name,
                stats->packets_dropped_reason[r->reason], stats->packets_dumped_reason[r->reason],
                conf_to_dump ? "[dump]" : "");
    } 
    fprintf(f, "\t%12s \t%10llu \t%10llu\n", "total", stats->packets_dropped, stats->packets_dumped);
}

