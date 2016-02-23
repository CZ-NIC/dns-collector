#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
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
    dns_output_write(out0, buf, (p - buf));
}


/**
 * Callback for cvs_output, writes singe CVS line.
 */
static dns_ret_t
dns_output_csv_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    char buf[DNS_OUTPUT_CVS_LINEMAX] = "";
    char addrbuf[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1] = "";
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
                case dns_of_client_addr:
                    inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_CLIENT_ADDR(pkt), addrbuf, sizeof(addrbuf));
                    p += sprintf(p, "%s", addrbuf);
                    break;
                case dns_of_client_port:
                    p += sprintf(p, "%d", DNS_PACKET_CLIENT_PORT(pkt));
                    break;
                case dns_of_server_addr:
                    inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_SERVER_ADDR(pkt), addrbuf, sizeof(addrbuf));
                    p += sprintf(p, "%s", addrbuf);
                    break;
                case dns_of_server_port:
                    p += sprintf(p, "%d", DNS_PACKET_SERVER_PORT(pkt));
                    break;
                case dns_of_id:
                    p += sprintf(p, "%d", ntohs(pkt->dns_data->id));
                    break;
                case dns_of_qname:
                    if (pkt->dns_qname_string)
                        p += sprintf(p, "%s", pkt->dns_qname_string);
                    else
                        *(p++) = '?';
                    break;
                case dns_of_qtype:
                    p += sprintf(p, "%d", pkt->dns_qtype);
                    break;
                case dns_of_qclass:
                    p += sprintf(p, "%d", pkt->dns_qclass);
                    break;
                case dns_of_request_time_us:
                    if (DNS_PACKET_REQUEST(pkt))
                        p += sprintf(p, "%"PRId64, DNS_PACKET_REQUEST(pkt)->ts);
                    break;
                case dns_of_request_flags:
                    if (DNS_PACKET_REQUEST(pkt))
                        p += sprintf(p, "%d", ntohs(DNS_PACKET_REQUEST(pkt)->dns_data->flags));
                    break;
                case dns_of_request_length:
                    if (DNS_PACKET_REQUEST(pkt))
                        p += sprintf(p, "%d", DNS_PACKET_REQUEST(pkt)->pkt_len);
                    break;
                case dns_of_response_time_us:
                    if (DNS_PACKET_RESPONSE(pkt))
                        p += sprintf(p, "%"PRId64, DNS_PACKET_RESPONSE(pkt)->ts);
                    break;
                case dns_of_response_flags:
                    if (DNS_PACKET_RESPONSE(pkt))
                        p += sprintf(p, "%d", ntohs(DNS_PACKET_RESPONSE(pkt)->dns_data->flags));
                    break;
                case dns_of_response_length:
                    if (DNS_PACKET_RESPONSE(pkt))
                        p += sprintf(p, "%d", DNS_PACKET_RESPONSE(pkt)->pkt_len);
                    break;
                default:
                    assert(0);
            }
            assert(p < buf + sizeof(buf));
        }
    }

    *(p++) = '\n';
    *(p) = '\0';
    dns_output_write(out0, buf, (p - buf));
    out0->wrote_items ++;

    return DNS_RET_OK;
}


/**
 * Helper for configuration init.
 */
static char *
dns_output_csv_conf_init(void *data)
{
    struct dns_output_csv *out = (struct dns_output_csv *) data;

    out->base.write_packet = dns_output_csv_write_packet;
    out->base.start_file = dns_output_csv_start_file;
    out->base.manage_files = 1;

    out->header = 1;
    out->separator = "|";

    return dns_output_init(&(out->base));
}


/**
 * Helper for configuration post-processing and validation.
 */
static char *
dns_output_csv_conf_commit(void *data)
{
    struct dns_output_csv *out = (struct dns_output_csv *) data;

    if (strlen(out->separator) != 1)
        return "'separator' needs to be exactly one character.";

    return dns_output_commit(&(out->base));
}


struct cf_section dns_output_csv_section = {
    CF_TYPE(struct dns_output_csv),
    .init = &dns_output_csv_conf_init,
    .commit = &dns_output_csv_conf_commit,
    CF_ITEMS {
        CF_DNS_OUTPUT_COMMON,
        CF_STRING("separator", PTR_TO(struct dns_output_csv, separator)),
        CF_INT("header", PTR_TO(struct dns_output_csv, header)),
        CF_BITMAP_LOOKUP("fields", PTR_TO(struct dns_output_csv, fields), dns_output_field_names),
        CF_END
    }
};

