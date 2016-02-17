#include <ucw/lib.h>
#include <ucw/conf.h>

#include "common.h"
#include "output.h"
#include "packet.h"


struct dns_output_csv {
    struct dns_output base;

    char *separator;
    int header;

    FILE *f;
    dns_us_time_t f_opened;
};


static dns_ret_t
dns_output_csv_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    // TODO: open, reopen, write
}


static void
dns_output_csv_close(struct dns_output *out0)
{
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    if (out->f)
        fclose(out->f);
}


static char *
dns_output_csv_conf_init(void *data)
{
    struct dns_output_csv *s = (struct dns_output_csv *) data;

    s->base.write_packet = dns_output_csv_write_packet;
    s->base.close = dns_output_csv_close;
    s->header = 1;
    s->separator = "|";

    return dns_output_init(&(s->base));
}


static char *
dns_output_csv_conf_commit(void *data)
{
    struct dns_output_csv *s = (struct dns_output_csv *) data;

    if (strlen(s->separator) != 1)
        return "'separator' needs to be exactly one character.";

    return dns_output_commit(&(s->base));
}


struct cf_section dns_output_csv_section = {
    CF_TYPE(struct dns_output_csv),
    .init = &dns_output_csv_conf_init,
    .commit = &dns_output_csv_conf_commit,
    CF_ITEMS {
        CF_STRING("path_template", PTR_TO(struct dns_output, path_template)),
        CF_DOUBLE("period", PTR_TO(struct dns_output, period_sec)),
        CF_STRING("separator", PTR_TO(struct dns_output_csv, separator)),
        CF_INT("header", PTR_TO(struct dns_output_csv, header)),
        CF_END
    }
};

