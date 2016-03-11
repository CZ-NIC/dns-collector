
#include <assert.h>
#include <stdio.h>

#include "stats.h"
#include "common.h"
#include "collector.h"

void
dns_stats_fprint(const dns_stats_t *stats, const struct dns_config *conf UNUSED, FILE *f)
{ 
    assert(stats && f);

    fprintf(f, "packets captured: \t%lu\n", stats->packets_captured);
    fprintf(f, "packets dropped and dumped:\n");
    for (enum dns_drop_reason r = 0; r < dns_drop_LAST; r++) {
        fprintf(f, "\t%12s \t%10lu \t%10lu\n", dns_drop_reason_names[r],
                stats->packets_dropped_reason[r], stats->packets_dumped_reason[r]);
    } 
    fprintf(f, "\t%12s \t%10lu \t%10lu\n", "total", stats->packets_dropped, stats->packets_dumped);
}

