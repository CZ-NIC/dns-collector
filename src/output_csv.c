/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    msg(L_INFO, "Selected fields: %#x", out->csv_fields);
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
 * Writes a packet to the given file, or writes field names if pkt == NULL.
 */
 
static size_t
write_packet(FILE *f, uint32_t fields, int separator, dns_packet_t *pkt)
{
    char addrbuf[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 4];
    int first = 1;
    char outbuf[2048];
    char *p = outbuf;

// Write fmt, args .. to the output (overflow safe)
#define WRITE(fmtargs...) \
        p += snprintf(p, outbuf + sizeof(outbuf) - p, fmtargs)
// Write impala NULL string to the output (overflow safe)
#define WRITENULL WRITE("\\N")
// Start a field conditional on the field flag being set,
// write just the field name for header if pkt==NULL
#define COND(fieldname) \
        if (fields & (1 << (dns_field_ ## fieldname))) { \
            if (first) { first = 0; } else { *(p++) = separator; } \
            if (!pkt) { WRITE( #fieldname ); } else { \
// Closing for COND(...) statement
#define COND_END  } }

    // Time

    COND(time)
        WRITE("%"PRId64".%06"PRId64, pkt->ts / 1000000L, pkt->ts % 1000000L);
    COND_END

    COND(delay_us) 
        if (pkt->response)
            WRITE("%"PRId64, pkt->response->ts - pkt->ts);
    COND_END

    // Sizes

    COND(req_dns_len)
        if (DNS_PACKET_REQUEST(pkt))
            WRITE("%zd", DNS_PACKET_REQUEST(pkt)->dns_data_size_orig);
    COND_END

    COND(resp_dns_len) 
        if (DNS_PACKET_RESPONSE(pkt)) 
            WRITE("%zd", DNS_PACKET_RESPONSE(pkt)->dns_data_size_orig);
    COND_END

    COND(req_net_len)
        if (DNS_PACKET_REQUEST(pkt)) 
            WRITE("%zd", DNS_PACKET_REQUEST(pkt)->net_size);
    COND_END

    COND(resp_net_len)
        if (DNS_PACKET_RESPONSE(pkt))
            WRITE("%zd", DNS_PACKET_RESPONSE(pkt)->net_size);
    COND_END

    // IP stats

    COND(client_addr)
        inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_CLIENT_ADDR(pkt), addrbuf, sizeof(addrbuf));
        WRITE("%s", addrbuf);
    COND_END

    COND(client_port) 
        WRITE("%d", DNS_PACKET_CLIENT_PORT(pkt));
    COND_END

    COND(server_addr)
        inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_SERVER_ADDR(pkt), addrbuf, sizeof(addrbuf));
        WRITE("%s", addrbuf);
    COND_END

    COND(server_port) 
        WRITE("%d", DNS_PACKET_SERVER_PORT(pkt));
    COND_END

    COND(net_proto) 
        WRITE("%d", pkt->net_protocol);
    COND_END

    COND(net_ipv) 
        switch (DNS_SOCKADDR_AF(&pkt->src_addr)) {
        case AF_INET:
            WRITE("4");
            break;
        case AF_INET6:
            WRITE("6");
            break;
        }
    COND_END

    COND(net_ttl) 
        if (pkt->net_ttl > 0)
            WRITE("%d", pkt->net_ttl);
    COND_END

    COND(req_udp_sum) 
        if ((pkt->net_protocol == IPPROTO_UDP) && DNS_PACKET_REQUEST(pkt)) 
            WRITE("%d", DNS_PACKET_REQUEST(pkt)->net_udp_sum);
    COND_END

    // DNS header

    COND(id) 
        WRITE("%d", knot_wire_get_id(pkt->knot_packet->wire));
    COND_END

    COND(qtype) 
        WRITE("%d", knot_pkt_qtype(pkt->knot_packet));
    COND_END

    COND(qclass) 
        WRITE("%d", knot_pkt_qclass(pkt->knot_packet));
    COND_END

    COND(opcode)
        WRITE("%d", knot_wire_get_opcode(pkt->knot_packet->wire));
    COND_END

    COND(rcode)
        if (DNS_PACKET_RESPONSE(pkt)) 
            WRITE("%d", knot_wire_get_rcode(DNS_PACKET_RESPONSE(pkt)->knot_packet->wire));
    COND_END

#define WRITEFLAG(type, name)  if (DNS_PACKET_ ## type (pkt)) \
    WRITE("%d", !! knot_wire_get_ ## name(DNS_PACKET_ ## type (pkt)->knot_packet->wire)); \

    COND(resp_aa) 
        WRITEFLAG(RESPONSE, aa);
    COND_END

    COND(resp_tc) 
        WRITEFLAG(RESPONSE, tc);
    COND_END

    COND(req_rd) 
        WRITEFLAG(REQUEST, rd);
    COND_END

    COND(resp_ra) 
        WRITEFLAG(RESPONSE, ra);
    COND_END

    COND(req_z) 
        WRITEFLAG(REQUEST, z);
    COND_END

    COND(resp_ad) 
        WRITEFLAG(RESPONSE, ad);
    COND_END

    COND(req_cd) 
        WRITEFLAG(REQUEST, cd);
    COND_END

    COND(qname) 
        char qname_buf[1024];
        char *res = knot_dname_to_str(qname_buf, knot_pkt_qname(pkt->knot_packet), sizeof(qname_buf));
        if (res) {
            WRITE("%s", res);
        } else {
            WRITENULL; 
        }
    COND_END

    COND(resp_ancount) 
        if (DNS_PACKET_RESPONSE(pkt)) 
            WRITE("%d", knot_wire_get_ancount(DNS_PACKET_RESPONSE(pkt)->dns_data));
    COND_END

    COND(resp_arcount) 
        if (DNS_PACKET_RESPONSE(pkt))
            WRITE("%d", knot_wire_get_arcount(DNS_PACKET_RESPONSE(pkt)->dns_data));
    COND_END

    COND(resp_nscount)
        if (DNS_PACKET_RESPONSE(pkt)) 
            WRITE("%d", knot_wire_get_nscount(DNS_PACKET_RESPONSE(pkt)->dns_data));
    COND_END

    // EDNS

    const knot_rrset_t *req_opt_rr = (pkt && DNS_PACKET_REQUEST(pkt)) ? DNS_PACKET_REQUEST(pkt)->knot_packet->opt_rr : NULL;
    const knot_rrset_t *resp_opt_rr = (pkt && DNS_PACKET_RESPONSE(pkt)) ? DNS_PACKET_RESPONSE(pkt)->knot_packet->opt_rr : NULL;

    COND(req_edns_ver) 
        if (req_opt_rr)
            WRITE("%d", knot_edns_get_version(req_opt_rr));
    COND_END

    COND(req_edns_udp) 
        if (req_opt_rr)
            WRITE("%d", knot_edns_get_payload(req_opt_rr));
    COND_END

    COND(req_edns_do) 
        if (req_opt_rr)
            WRITE("%d", !! knot_edns_do(req_opt_rr));
    COND_END

    COND(resp_edns_rcode) 
        if (resp_opt_rr)
            WRITE("%d", knot_edns_get_ext_rcode(resp_opt_rr));
    COND_END

#define WRITE_UNDERSTOOD_LIST(code) \
    if (req_opt_rr) { \
        uint8_t *opt = knot_edns_get_option(req_opt_rr, code); \
        if (opt) { for (int i = 0; i < knot_edns_opt_get_length(opt); i++) { \
            if (i > 0) WRITE( (separator == ',') ? "\\," : "," ); \
            WRITE("%d", (int)((opt + 2 * sizeof(uint16_t) + i)[i])); \
        } } else { WRITENULL; } \
    } else { WRITENULL; }

    COND(req_edns_ping)
        // TODO: distinguish a PING request from DAU,
        // see https://github.com/SIDN/entrada/blob/b787af190267df148683151638ce94508bd6139e/dnslib4java/src/main/java/nl/sidn/dnslib/message/records/edns0/OPTResourceRecord.java#L146
    COND_END

    COND(req_edns_dau)
        WRITE_UNDERSTOOD_LIST(5) /* DAU */
    COND_END

    COND(req_edns_dhu)
        WRITE_UNDERSTOOD_LIST(6) /* DHU */
    COND_END

    COND(req_edns_n3u)
        WRITE_UNDERSTOOD_LIST(7) /* N3U */
    COND_END

    COND(resp_edns_nsid) 
        if (resp_opt_rr) {
            uint8_t *opt = knot_edns_get_option(resp_opt_rr, 3); /* NSID */
            if (opt) {
                p += dns_snescape(p, (outbuf + sizeof(outbuf)) - p, separator,
                                  opt + 2 * sizeof(uint16_t), knot_edns_opt_get_length(opt));
            } else { WRITENULL; }
        } else { WRITENULL; }
    COND_END

    COND(edns_client_subnet)
        uint8_t *opt = NULL;
        if (resp_opt_rr)
            opt = knot_edns_get_option(resp_opt_rr, 8); /* client subnet from response */
        if (req_opt_rr && !opt)
            opt = knot_edns_get_option(req_opt_rr, 8); /* client subnet from request */
        if (opt) {
            // TODO: PARSE and PRINT client subnet information
            // As in: "4,118.71.70/24,0"
            // Entrada: (fam == 1? "4,": "6,") + address + "/" + sourcenetmask + "," + scopenetmask;
        } else { WRITENULL; }
    COND_END

    COND(edns_other)
        const knot_rrset_t *opt_rr = resp_opt_rr;
        if (req_opt_rr)
            opt_rr = req_opt_rr; // Prefer request OPT RR
        if (opt_rr) {
            // TODO: PARSE and PRINT options other than used above
        } else { WRITENULL; }
    COND_END

#undef WRITE
#undef WRITENULL
#undef COND
#undef COND_END
#undef WRITEFLAG
#undef WRITE_UNDERSTOOD_LIST

    // step back when buffer is full, warn
    if (p > outbuf + sizeof(outbuf) - 2) {
        p = outbuf + sizeof(outbuf) - 2;
        msg(L_WARN | DNS_MSG_SPAM, "CSV line too long (more than %zd) - truncated", sizeof(outbuf) - 2);
    }
    *(p++) = '\n';
    *(p++) = '\0';

    fwrite(outbuf, p - outbuf - 1, 1, f);
    return p - outbuf - 1;
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
        out->base.current_bytes += write_packet(out->base.out_file, out->csv_fields, out->separator, NULL);

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
        write_packet(headerf, out->csv_fields, out->separator, NULL);
        fclose(headerf);
    }
}


/**
 * Callback for cvs_output, writes singe CVS line.
 */
static dns_ret_t
dns_output_csv_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    assert(out0 && pkt);
    struct dns_output_csv *out = (struct dns_output_csv *) out0;
    size_t n = write_packet(out->base.out_file, out->csv_fields, out->separator, pkt);
    out->base.current_bytes += n;

    // Accounting
    out->base.current_items ++;
    if (!DNS_PACKET_RESPONSE(pkt))
        out->base.current_request_only ++;
    if (!DNS_PACKET_REQUEST(pkt))
        out->base.current_response_only ++;

    return DNS_RET_OK;
}
