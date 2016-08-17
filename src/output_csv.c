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


// Forward declarations

static size_t
dns_output_csv_write_header(struct dns_output_csv *out, FILE *file);

static void
dns_output_csv_start_file(struct dns_output *out0, dns_us_time_t time);

static dns_ret_t
dns_output_csv_write_packet(struct dns_output *out0, dns_packet_t *pkt);


struct dns_output_csv *
dns_output_csv_create(struct dns_config *conf, struct dns_frame_queue *in)
{
    struct dns_output_csv *out = xmalloc_zero(sizeof(struct dns_output_csv));

    dns_output_init(&out->base, in, conf->output_path_fmt, conf->output_pipe_cmd, conf->output_period_sec);
    out->base.start_file = dns_output_csv_start_file;
    out->base.write_packet = dns_output_csv_write_packet;

    out->separator = conf->csv_separator[0];
    out->inline_header = conf->csv_inline_header;
    out->csv_fields = conf->csv_fields;
    if (conf->csv_external_header_path_fmt)
        out->external_header_path_fmt = strdup(conf->csv_external_header_path_fmt);
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
        out->base.current_bytes += dns_output_csv_write_header(out, out->base.out_file);

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
    char outbuf[1024];
    char *p = outbuf;

//        if (first) { first = 0; } else { putc(out->separator, out->base.out_file); out->base.current_bytes += 1; } 
#define COND(flag) \
        if (out->csv_fields & (1 << (flag)))
#define WRITEFIELD0 \
        if (first) { first = 0; } else { *(p++) = out->separator; } 
#define WRITEFIELD(fmtargs...) \
        WRITEFIELD0 \
        p += snprintf(p, (outbuf + sizeof(outbuf)) - p, fmtargs);

    // Time

    COND(dns_of_timestamp) {
        WRITEFIELD("%"PRId64".%06"PRId64, pkt->ts / 1000000L, pkt->ts % 1000000L);
    }

    COND(dns_of_delay_us) {
        if (pkt->response) {
            WRITEFIELD("%"PRId64, pkt->response->ts - pkt->ts);
        } else {
            WRITEFIELD0;
        }
    }

    // Sizes

    COND(dns_of_req_dns_len) {
        if (DNS_PACKET_REQUEST(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_REQUEST(pkt)->dns_data_size_orig);
        } else {
            WRITEFIELD("%d", 0);
        }
    }

    COND(dns_of_resp_dns_len) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_RESPONSE(pkt)->dns_data_size_orig);
        } else {
            WRITEFIELD("%d", 0);
        }
    }

    COND(dns_of_req_net_len) {
        if (DNS_PACKET_REQUEST(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_REQUEST(pkt)->net_size);
        } else {
            WRITEFIELD("%d", 0);
        }
    }

    COND(dns_of_resp_net_len) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_RESPONSE(pkt)->net_size);
        } else {
            WRITEFIELD("%d", 0);
        }
    }

    // IP stats

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

    COND(dns_of_net_proto) {
        WRITEFIELD("%d", pkt->net_protocol);
    }

    COND(dns_of_net_ipv) {
        switch (DNS_SOCKADDR_AF(&pkt->src_addr)) {
        case AF_INET:
            WRITEFIELD("4");
            break;
        case AF_INET6:
            WRITEFIELD("6");
            break;
        default:
            WRITEFIELD0;
            break;
        }
    }

    COND(dns_of_net_ttl) {
        if (pkt->net_ttl > 0) {
            WRITEFIELD("%d", pkt->net_ttl);
        } else {
            WRITEFIELD0;
        }
    }

    COND(dns_of_req_udp_sum) {
        if ((pkt->net_protocol == IPPROTO_UDP) && DNS_PACKET_REQUEST(pkt)) {
            WRITEFIELD("%d", DNS_PACKET_REQUEST(pkt)->net_udp_sum);
        } else {
            WRITEFIELD0;
        }
    }

    // DNS header

    COND(dns_of_id) {
        WRITEFIELD("%d", knot_wire_get_id(pkt->knot_packet->wire));
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

    /* TODO: better escaping: impala-compatible? */
    COND(dns_of_qname) {
        char qname_buf[512];
        char * res = knot_dname_to_str(qname_buf, knot_pkt_qname(pkt->knot_packet), sizeof(qname_buf));
        WRITEFIELD("%s", res ? res : "INVALID_QNAME" );
    }

    COND(dns_of_resp_ancount) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%d", knot_wire_get_ancount(DNS_PACKET_RESPONSE(pkt)->dns_data));
        } else {
            WRITEFIELD0;
        }
    }

    COND(dns_of_resp_arcount) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%d", knot_wire_get_arcount(DNS_PACKET_RESPONSE(pkt)->dns_data));
        } else {
            WRITEFIELD0;
        }
    }

    COND(dns_of_resp_nscount) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%d", knot_wire_get_nscount(DNS_PACKET_RESPONSE(pkt)->dns_data));
        } else {
            WRITEFIELD0;
        }
    }

#undef COND
#undef WRITEFIELD

    *(p++) = '\n';
    *(p++) = '\0';

    fwrite(outbuf, p - outbuf - 1, 1, out->base.out_file);
    out->base.current_bytes += p - outbuf - 1;

    // Accounting
    out->base.current_items ++;
    if (!DNS_PACKET_RESPONSE(pkt))
        out->base.current_request_only ++;
    if (!DNS_PACKET_REQUEST(pkt))
        out->base.current_response_only ++;

    return DNS_RET_OK;
}

