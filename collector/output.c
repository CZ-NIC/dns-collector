#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>

#include "common.h"
#include "output.h"


char *dns_output_init(struct dns_output *out)
{
    out->period_sec = 300.0; // 5 min

    return NULL;
}


char *dns_output_commit(struct dns_output *out)
{
    if (out->period_sec < 0.000001)
        out->period = 0;
    else
        out->period = dns_fsec_to_us_time(out->period_sec);

    if (!out->path_template)
        return "'path_template' needed in output.";

    return NULL;
}


void
dns_output_open(struct dns_output *out, dns_us_time_t time)
{
    assert(out && !out->f && out->manage_files && (time != DNS_NO_TIME));

    char path[PATH_MAX];
    size_t l = dns_us_time_strftime(path, PATH_MAX, out->path_template, time);

    if (l == 0)
        die("Expanded filename '%s' too long.", out->path_template);
        
    out->f = fopen(path, "w");
    if (!out->f)
        die("Unable to open output file '%s': %s.", path, strerror(errno));
    out->f_time_opened = time;

    if (out->start_file)
          out->start_file(out, time);
}


void
dns_output_close(struct dns_output *out, dns_us_time_t time)
{
    assert(out && out->f && out->manage_files && time != DNS_NO_TIME);

    if (out->finish_file)
          out->finish_file(out, time);

    fclose(out->f);
    out->f = NULL;
}


void
dns_output_check_rotation(struct dns_output *out, dns_us_time_t time)
{
    assert(out && (time != DNS_NO_TIME) && out->manage_files);

    // check if we need to switch output files
    if ((out->period > 0) && (out->f) && (time >= out->f_time_opened + out->period))
        dns_output_close(out, time);

    // open if not open
    if (!out->f)
        dns_output_open(out, time);
}
// TODO: Sync wile rotation with respect to drop/write

void
dns_output_write(struct dns_output *out, const char *buf, size_t len)
{
    assert(out && buf);

    if ((len > 1) && (out->f)) {
        size_t l = fwrite(buf, len, 1, out->f);
        if (l != 1) {
            msg(L_ERROR, "IO error %d writing to an output file.", ferror(out->f));
            // TODO: Do something else?
        }
    }
}


