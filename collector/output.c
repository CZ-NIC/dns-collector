#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>


#include "common.h"
#include "output.h"


const char *dns_output_field_names[] = {
    "flags",
    "client_addr",
    "client_port",
    "server_addr",
    "server_port",
    "id",
    "qname",
    "qtype",
    "qclass",
    "request_time_us",
    "request_flags",
    "request_length",
    "response_time_us",
    "response_flags",
    "response_length",
    NULL,
};


char *dns_output_init(struct dns_output *s)
{
    s->period_sec = 300.0; // 5 min

    return NULL;
}


char *dns_output_commit(struct dns_output *s)
{
    if (s->period_sec < 0.000001)
        s->period = 0;
    else
        s->period = dns_fsec_to_us_time(s->period_sec);

    if (!s->path_template)
        return "'path_template' needed in output.";

    return NULL;
}


void
dns_output_open(struct dns_output *out, dns_us_time_t time)
{
    assert(out && !out->f && out->manage_files);

    if (time == DNS_NO_TIME) {
        msg(L_ERROR, "[BUG] Unable to open file with time==DNS_NO_TIME.");
        return;
    }

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
dns_output_write(struct dns_output *out, const char *buf, size_t len, dns_us_time_t time)
{
    assert(out && buf && out->manage_files);

    // check if we need to switch output files
    if ((time != DNS_NO_TIME) && (out->f) && (time >= out->f_time_opened + out->period))
        dns_output_close(out, time);

    // open output if none open
    if (!out->f) {
        dns_output_open(out, time);
        if (!out->f)
            return;
    }

    if (len > 1) {
        size_t l = fwrite(buf, len, 1, out->f);
        if (l != 1) {
            msg(L_ERROR, "IO error %d writing to an output file.", ferror(out->f));
            // TODO: Do something else?
        }
    }
}


