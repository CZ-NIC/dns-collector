#include <time.h>
#include <ucw/lib.h>
#include <ucw/conf.h>

#include "common.h"
#include "output.h"
#include "packet.h"


struct dns_output_csv {
    struct dns_output base;

    char *separator;
    int header;
    uint32_t fields;
};

#define DNS_OUTPUT_CVS_LINEMAX 512

/**
 * Callback for cvs_output, writes CVS header.
 */
void
dns_output_csv_start_file(struct dns_output *out0, dns_us_time_t time UNUSED)
{
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    char buf[DNS_OUTPUT_CVS_LINEMAX] = "";
    char *p = buf;
    int first = 1;

    for (int f = 0; f < dns_of_LAST; f++) {
        if (out->fields & (1 << f)) {
            if (!first)
                *(p++) = '|';
            else
                first = 0;

            size_t l = strlen(dns_output_field_names[f]);
            memcpy(p, dns_output_field_names[f], l);
            p += l;
        }
    }

    *(p++) = '\n';
    *(p) = '\0';
    dns_output_write(out0, buf, (p - buf), DNS_NO_TIME);
}


/**
 * Callback for cvs_output, writes singe CVS line.
 */
static dns_ret_t
dns_output_csv_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    char buf[DNS_OUTPUT_CVS_LINEMAX] = "";
    char *p = buf;
    int first = 1;

    for (int f = 0; f < dns_of_LAST; f++) {
        if (out->fields & (1 << f)) {
            if (!first)
                *(p++) = '|';
            else
                first = 0;

            switch (f) {
                case dns_of_flags:
                    p += sprintf(p, "%d", dns_packet_get_output_flags(pkt));
                    break;
                case dns_of_qname:
                    p += sprintf(p, "%s", pkt->dns_qname_string);
                    break;
                // TODO: implement the rest
                default:
                    *(p++) = '?';
            }
        }
    }

    *(p++) = '\n';
    *(p) = '\0';
    dns_output_write(out0, buf, (p - buf), pkt->ts);

    return DNS_RET_OK;
}


/**
 * Helper for configuration init.
 */
static char *
dns_output_csv_conf_init(void *data)
{
    struct dns_output_csv *s = (struct dns_output_csv *) data;

    s->base.write_packet = dns_output_csv_write_packet;
    s->base.start_file = dns_output_csv_start_file;
    s->base.manage_files = 1;

    s->header = 1;
    s->separator = "|";

    return dns_output_init(&(s->base));
}


/**
 * Helper for configuration post-processing and validation.
 */
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
        CF_BITMAP_LOOKUP("fields", PTR_TO(struct dns_output_csv, fields), dns_output_field_names),
        CF_END
    }
};

