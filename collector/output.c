#include "common.h"
#include "output.h"


char *dns_output_init(struct dns_output *s)
{
    s->period_sec = 300.0; // 5 min

    return NULL;
}


char *dns_output_commit(struct dns_output *s)
{
    if (s->period_sec < 0.001)
        return "'period' too small, minimum 0.001 sec.";
    s->period = dns_fsec_to_us_time(s->period_sec);

    if (!s->path_template)
        return "'path_template' needed in output.";

    return NULL;
}

