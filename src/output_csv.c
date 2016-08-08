#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "output.h"
#include "packet.h"
#include "output_csv.h"

/**
 * Helper to write CSV header to a file, return num of bytes
 */
static size_t
dns_output_csv_write_header(struct dns_output_csv *out, FILE *file)
{
    size_t n = 0;
    int first = 1;
    for (int f = 0; f < dns_of_LAST; f++) {
        if (out->csv_fields & (1 << f)) {
            if (first)
                first = 0;
            else {
                fputc(out->separator, file);
                n++;
            }
            fputs(dns_output_field_names[f], file);
            n += strlen(dns_output_field_names[f]);
        }
    }
    fputc('\n', file);
    n++;
    return n;
}

/**
 * Callback for cvs_output, writes CVS header.
 */
static void
dns_output_csv_start_file(struct dns_output *out0, dns_us_time_t time)
{
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    assert(out && out->base.out_file);

    if (out->inline_header)
        out->base.wrote_bytes += dns_output_csv_write_header(out, out->base.out_file);

    if (out->external_header_path_fmt && strlen(out->external_header_path_fmt) > 0) {
        int path_len = strlen(out->external_header_path_fmt) + DNS_OUTPUT_FILENAME_EXTRA;
        char *path = alloca(path_len);
        size_t l = dns_us_time_strftime(path, path_len, out->external_header_path_fmt, time);
        if (l == 0)
            die("Expanded filename '%s' expansion too long.", out->external_header_path_fmt);

        FILE *headerf = fopen(path, "w");
        if (!headerf) {
            die("Unable to open output header file '%s': %s.", path, strerror(errno));
        }
        dns_output_csv_write_header(out, headerf);
        fclose(headerf);
    }
}


/**
 * Callback for cvs_output, writes singe CVS line.
 */
static dns_ret_t
dns_output_csv_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    char addrbuf[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1] = "";
    int first = 1;

#define COND(flag) \
        if (out->csv_fields & (1 << (flag)))
#define WRITEFIELD(fmt, args...) \
        if (first) { first = 0; } else { putc(out->separator, out->base.out_file); out->base.wrote_bytes += 1; } \
        out->base.wrote_bytes += fprintf(out->base.out_file, fmt, args);

    COND(dns_of_timestamp) {
        WRITEFIELD("%"PRId64".%06"PRId64, pkt->ts / 1000000L, pkt->ts % 1000000L);
    }

    COND(dns_of_client_addr) {
        inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_CLIENT_ADDR(pkt), addrbuf, sizeof(addrbuf));
        WRITEFIELD("%s", addrbuf);
    }

    COND(dns_of_client_port) {
        WRITEFIELD("%d", DNS_PACKET_CLIENT_PORT(pkt));
    }

    COND(dns_of_server_addr) {
        inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_SERVER_ADDR(pkt), addrbuf, sizeof(addrbuf));
        WRITEFIELD("%s", addrbuf);
    }

    COND(dns_of_server_port) {
        WRITEFIELD("%d", DNS_PACKET_SERVER_PORT(pkt));
    }

    COND(dns_of_id) {
        WRITEFIELD("%d", knot_wire_get_id(pkt->knot_packet->wire));
    }

    COND(dns_of_qname) {
        char qname_buf[512];
        char * res = knot_dname_to_str(qname_buf, knot_pkt_qname(pkt->knot_packet), sizeof(qname_buf));
        WRITEFIELD("%s", res ? res : "INVALID_QNAME" );
    }

    COND(dns_of_qtype) {
        WRITEFIELD("%d", knot_pkt_qtype(pkt->knot_packet));
    }

    COND(dns_of_qclass) {
        WRITEFIELD("%d", knot_pkt_qclass(pkt->knot_packet));
    }

    COND(dns_of_flags) {
        struct dns_packet *pp = pkt;
        if (pkt->response)
            pp = pkt->response;
        WRITEFIELD("%d", (knot_wire_get_flags1(pp->knot_packet->wire) << 8) + knot_wire_get_flags2(pp->knot_packet->wire))
    }
/*
    CONDWRITE(dns_of_request_ans_rrs) {
        if (DNS_PACKET_REQUEST(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_REQUEST(pkt)->dns_data->ans_rrs));
    }

    CONDWRITE(dns_of_request_auth_rrs) {
        if (DNS_PACKET_REQUEST(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_REQUEST(pkt)->dns_data->auth_rrs));
    }

    CONDWRITE(dns_of_request_add_rrs) {
        if (DNS_PACKET_REQUEST(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_REQUEST(pkt)->dns_data->add_rrs));
    }
*/
    COND(dns_of_request_length) {
        if (DNS_PACKET_REQUEST(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_REQUEST(pkt)->dns_data_size_orig);
        } else {
            WRITEFIELD("%d", -1);
        }
    }
/*
    CONDWRITE(dns_of_response_time_us) {
        if (DNS_PACKET_RESPONSE(pkt))
            p += sprintf(p, "%"PRId64, DNS_PACKET_RESPONSE(pkt)->ts);
    }

    CONDWRITE(dns_of_response_flags) {
        if (DNS_PACKET_RESPONSE(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_RESPONSE(pkt)->dns_data->flags));
    }

    CONDWRITE(dns_of_response_ans_rrs) {
        if (DNS_PACKET_RESPONSE(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_RESPONSE(pkt)->dns_data->ans_rrs));
    }

    CONDWRITE(dns_of_response_auth_rrs) {
        if (DNS_PACKET_RESPONSE(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_RESPONSE(pkt)->dns_data->auth_rrs));
    }

    CONDWRITE(dns_of_response_add_rrs) {
        if (DNS_PACKET_RESPONSE(pkt))
            p += sprintf(p, "%d", ntohs(DNS_PACKET_RESPONSE(pkt)->dns_data->add_rrs));
    }
*/
    COND(dns_of_response_length) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_RESPONSE(pkt)->dns_data_size_orig);
        } else {
            WRITEFIELD("%d", -1);
        }
    }

    COND(dns_of_delay_us) {
        if (pkt->response) {
            WRITEFIELD("%"PRId64, pkt->response->ts - pkt->ts);
        } else {
            WRITEFIELD("%d", -1);
        }
    }

#undef COND
#undef WRITEFIELD

    putc('\n', out->base.out_file);
    out->base.wrote_bytes += 1;
    out->base.wrote_packets ++;

    return DNS_RET_OK;
}

// Outer interface

struct dns_output_csv *
dns_output_csv_create(struct dns_frame_queue *in, const struct dns_output_csv_config *conf)
{
    struct dns_output_csv *out = xmalloc_zero(sizeof(struct dns_output_csv));

    dns_output_init(&out->base, in, conf->path_fmt, conf->pipe_cmd, conf->period_sec);
    out->base.start_file = dns_output_csv_start_file;
    out->base.write_packet = dns_output_csv_write_packet;

    out->separator = conf->separator[0];
    out->inline_header = conf->inline_header;
    out->csv_fields = conf->csv_fields;
    if (conf->external_header_path_fmt)
        out->external_header_path_fmt = strdup(conf->external_header_path_fmt);
    return out;
}

void
dns_output_csv_start(struct dns_output_csv *out)
{
    dns_output_start(&out->base);
}

void
dns_output_csv_finish(struct dns_output_csv *out)
{
    dns_output_finish(&out->base);
}

void
dns_output_csv_destroy(struct dns_output_csv *out)
{
    dns_output_finalize(&out->base);
    if (out->external_header_path_fmt)
        free(out->external_header_path_fmt);
    free(out);
}

/**
 * Helper for configuration init.
 */
static char *
dns_output_csv_config_init(void *data)
{
    struct dns_output_csv_config *conf = (struct dns_output_csv_config *) data;

    conf->path_fmt = NULL;
    conf->pipe_cmd = NULL;
    conf->period_sec = 0;
    conf->separator = ",";
    conf->inline_header = 0;
    conf->external_header_path_fmt = NULL;
    conf->csv_fields = 0; // TODO: some other default?
    return NULL;
}

/**
 * Helper for configuration post-processing and validation.
 */
static char *
dns_output_csv_config_commit(void *data)
{
    struct dns_output_csv_config *conf = (struct dns_output_csv_config *) data;

    if (strlen(conf->separator) != 1)
        return "'separator' needs to be exactly one character.";
    if ((conf->separator[0] < 0x20) || (conf->separator[0] > 0x7f))
        return "'separator' should be a printable, non-whitespace character.";
    if (conf->csv_fields == 0)
        return "Set output_csv.csv_fields to contain at least one field.";

    return NULL;
}

/**
 * Libucw configuration subsection definition.
 */
struct cf_section dns_output_csv_section = {
    CF_TYPE(struct dns_output_csv_config),
    .init = &dns_output_csv_config_init,
    .commit = &dns_output_csv_config_commit,
    CF_ITEMS {
        CF_STRING("path_fmt", PTR_TO(struct dns_output_csv_config, path_fmt)),
        CF_STRING("pipe_cmd", PTR_TO(struct dns_output_csv_config, pipe_cmd)),
        CF_INT("period_seconds", PTR_TO(struct dns_output_csv_config, period_sec)),
        CF_STRING("separator", PTR_TO(struct dns_output_csv_config, separator)),
        CF_INT("inline_header", PTR_TO(struct dns_output_csv_config, inline_header)),
        CF_STRING("external_header_path_fmt", PTR_TO(struct dns_output_csv_config, external_header_path_fmt)),
        CF_BITMAP_LOOKUP("fields", PTR_TO(struct dns_output_csv_config, csv_fields), dns_output_field_names),
        CF_END
    }
};


