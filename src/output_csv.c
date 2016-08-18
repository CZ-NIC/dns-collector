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
#define WRITE(fmtargs...) \
        p += snprintf(p, (outbuf + sizeof(outbuf)) - p, fmtargs);
#define WRITEFIELD(fmtargs...) \
        WRITEFIELD0 \
        WRITE(fmtargs)

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
            WRITEFIELD0;
        }
    }

    COND(dns_of_resp_dns_len) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_RESPONSE(pkt)->dns_data_size_orig);
        } else {
            WRITEFIELD0;
        }
    }

    COND(dns_of_req_net_len) {
        if (DNS_PACKET_REQUEST(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_REQUEST(pkt)->net_size);
        } else {
            WRITEFIELD0;
        }
    }

    COND(dns_of_resp_net_len) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%zd", DNS_PACKET_RESPONSE(pkt)->net_size);
        } else {
            WRITEFIELD0;
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

    COND(dns_of_opcode) {
        WRITEFIELD("%d", knot_wire_get_opcode(pkt->knot_packet->wire));
    }

    COND(dns_of_rcode) {
        if (DNS_PACKET_RESPONSE(pkt)) {
            WRITEFIELD("%d", knot_wire_get_rcode(DNS_PACKET_RESPONSE(pkt)->knot_packet->wire));
        } else {
            WRITEFIELD0;
        }
    }

#define WRITEFLAG(type, name)  if (DNS_PACKET_ ## type (pkt)) { \
    WRITEFIELD("%d", !! knot_wire_get_ ## name(DNS_PACKET_ ## type (pkt)->knot_packet->wire)); \
    } else { WRITEFIELD0; }

    COND(dns_of_resp_aa) {
        WRITEFLAG(RESPONSE, aa);
    }
    COND(dns_of_resp_tc) {
        WRITEFLAG(RESPONSE, tc);
    }
    COND(dns_of_req_rd) {
        WRITEFLAG(REQUEST, rd);
    }
    COND(dns_of_resp_ra) {
        WRITEFLAG(RESPONSE, ra);
    }
    COND(dns_of_req_z) {
        WRITEFLAG(REQUEST, z);
    }
    COND(dns_of_resp_ad) {
        WRITEFLAG(RESPONSE, ad);
    }
    COND(dns_of_req_cd) {
        WRITEFLAG(REQUEST, cd);
    }
#undef WRITEFLAG

    /* TODO: check escaping, impala-compatible */
    COND(dns_of_qname) {
        char qname_buf[512];
        char * res = knot_dname_to_str(qname_buf, knot_pkt_qname(pkt->knot_packet), sizeof(qname_buf));
        WRITEFIELD("%s", res ? res : "\\N" ); // Impala NULL string
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

    // EDNS

    const knot_rrset_t *opt_rr = pkt->knot_packet->opt_rr;

#define COND_EDNS(flag) COND(flag) { if (!opt_rr) { WRITEFIELD0; } else 
#define COND_EDNS_END }

    COND_EDNS(dns_of_edns_version) {
        WRITEFIELD("%d", knot_edns_get_version(opt_rr));
    } COND_EDNS_END
    COND_EDNS(dns_of_edns_udp) {
        WRITEFIELD("%d", knot_edns_get_payload(opt_rr));
    } COND_EDNS_END
    COND_EDNS(dns_of_edns_do) {
        WRITEFIELD("%d", !! knot_edns_do(opt_rr));
    } COND_EDNS_END
    cond_edns(dns_of_edns_ping) {
        writefield("%d", !! knot_edns_has_option(opt_rr));
    } cond_edns_end
    cond_edns(dns_of_edns_ext_rcode) {
        writefield("%d", knot_edns_get_ext_rcode(opt_rr));
    } cond_edns_end

    // TODO: list separator "," and escaping (and use "|" or "\t" in CSV as the default
#define WRITE_OPTBYTELIST(code) uint8_t *opt = knot_edns_get_option(opt_rr, code); \
    WRITEFIELD0; \
    if (opt) { for (int i = 0; i < knot_edns_opt_get_length(opt); i++) { if (i > 0) WRITE(" "); WRITE("%d", (int)(opt + 2 * sizeof(uint16_t) + i)); } }

    // TODO: distinguish a PING request from DAU,
    // see https://github.com/SIDN/entrada/blob/b787af190267df148683151638ce94508bd6139e/dnslib4java/src/main/java/nl/sidn/dnslib/message/records/edns0/OPTResourceRecord.java#L146
    COND_EDNS(dns_of_edns_dnssec_dau) {
        WRITE_OPTBYTELIST(5) /* DAU */
    } COND_EDNS_END
    COND_EDNS(dns_of_edns_dnssec_dhu) {
        WRITE_OPTBYTELIST(6) /* DHU */
    } COND_EDNS_END
    COND_EDNS(dns_of_edns_dnssec_n3u) {
        WRITE_OPTBYTELIST(7) /* N3U */
    } COND_EDNS_END

#undef WRITEFIELD
#undef WRITEFIELD0
#undef WRITE
#undef WRITE_OPTBYTES
#undef COND
#undef COND_EDNS
#undef COND_EDNS_END

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

