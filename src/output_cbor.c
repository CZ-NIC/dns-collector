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
#include <cbor.h>

#include "common.h"
#include "output.h"
#include "packet.h"
#include "output_cbor.h"


/**
 * Writes a packet to the given file, or writes header if pkt == NULL.
 */
 
static size_t
write_packet(FILE *f, uint32_t fields, dns_packet_t *pkt)
{
    char addrbuf[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 4];
    uint8_t outbuf[4096];
    CborEncoder ebase, eitem;

// Run cmd and die (with an error meaasge) on any CBOR error
#define CERR(cmd) { CborError cerr__ = (cmd); \
        if (cerr__ != CborNoError) { die("CBOR error: %s", cbor_error_string(cerr__)); } }

    cbor_encoder_init(&ebase, outbuf, sizeof(outbuf), 0);
    int noitems = __builtin_popcount(fields);
    if (fields & (1 << dns_field_flags)) noitems += 6;
    if (fields & (1 << dns_field_rr_counts)) noitems += 2;
    if (fields & (1 << dns_field_edns)) noitems += 9;
    CERR(cbor_encoder_create_array(&ebase, &eitem, noitems));

// Start writing a field conditional on the field flag being set,
// write just the field name for the header if pkt==NULL
// write NONE if secondary is not satisfied
#define CONDIF(fieldname, secondary) \
        if (fields & (1 << (dns_field_ ## fieldname))) { \
            if (!(pkt)) { CERR(cbor_encode_text_stringz(&eitem, #fieldname )); } else { \
                if (!(secondary)) { CERR(cbor_encode_null(&eitem)); } else {

#define COND(fieldname) CONDIF(fieldname, 1)

// Closing for COND(...) statement
#define COND_END  } } }

    // Time

    COND(time)
        CERR(cbor_encode_double(&eitem, (double)(pkt->ts) / 1000000.0));
    COND_END

    CONDIF(delay_us, pkt->response) 
        CERR(cbor_encode_int(&eitem, pkt->response->ts - pkt->ts));
    COND_END

    // Sizes

    CONDIF(req_dns_len, DNS_PACKET_REQUEST(pkt))
        CERR(cbor_encode_int(&eitem, DNS_PACKET_REQUEST(pkt)->dns_data_size_orig));
    COND_END

    CONDIF(resp_dns_len, DNS_PACKET_RESPONSE(pkt)) 
        CERR(cbor_encode_int(&eitem, DNS_PACKET_RESPONSE(pkt)->dns_data_size_orig));
    COND_END

    CONDIF(req_net_len, DNS_PACKET_REQUEST(pkt))
        CERR(cbor_encode_int(&eitem, DNS_PACKET_REQUEST(pkt)->net_size));
    COND_END

    CONDIF(resp_net_len, DNS_PACKET_RESPONSE(pkt))
        CERR(cbor_encode_int(&eitem, DNS_PACKET_RESPONSE(pkt)->net_size));
    COND_END

    // IP stats

    COND(client_addr)
        inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_CLIENT_ADDR(pkt), addrbuf, sizeof(addrbuf));
        CERR(cbor_encode_text_stringz(&eitem, addrbuf));
    COND_END

    COND(client_port) 
        CERR(cbor_encode_int(&eitem, DNS_PACKET_CLIENT_PORT(pkt)));
    COND_END

    COND(server_addr)
        inet_ntop(DNS_PACKET_AF(pkt), DNS_PACKET_SERVER_ADDR(pkt), addrbuf, sizeof(addrbuf));
        CERR(cbor_encode_text_stringz(&eitem, addrbuf));
    COND_END

    COND(server_port) 
        CERR(cbor_encode_int(&eitem, DNS_PACKET_SERVER_PORT(pkt)));
    COND_END

    COND(net_proto) 
        CERR(cbor_encode_int(&eitem, pkt->net_protocol));
    COND_END

    COND(net_ipv) 
        switch (DNS_SOCKADDR_AF(&pkt->src_addr)) {
        case AF_INET:
            CERR(cbor_encode_int(&eitem, 4));
            break;
        case AF_INET6:
            CERR(cbor_encode_int(&eitem, 6));
            break;
        }
    COND_END

    CONDIF(net_ttl, pkt->net_ttl > 0) 
        CERR(cbor_encode_int(&eitem, pkt->net_ttl));
    COND_END

    CONDIF(req_udp_sum, (pkt->net_protocol == IPPROTO_UDP) && DNS_PACKET_REQUEST(pkt)) 
        CERR(cbor_encode_int(&eitem, DNS_PACKET_REQUEST(pkt)->net_udp_sum));
    COND_END

    // DNS header

    COND(id) 
        CERR(cbor_encode_int(&eitem, knot_wire_get_id(pkt->knot_packet->wire)));
    COND_END

    COND(qtype) 
        CERR(cbor_encode_int(&eitem, knot_pkt_qtype(pkt->knot_packet)));
    COND_END

    COND(qclass) 
        CERR(cbor_encode_int(&eitem, knot_pkt_qclass(pkt->knot_packet)));
    COND_END

    COND(opcode)
        CERR(cbor_encode_int(&eitem, knot_wire_get_opcode(pkt->knot_packet->wire)));
    COND_END

    CONDIF(rcode, DNS_PACKET_RESPONSE(pkt))
            CERR(cbor_encode_int(&eitem, knot_wire_get_rcode(DNS_PACKET_RESPONSE(pkt)->knot_packet->wire)));
    COND_END

#define WRITEFLAG(label, type, name) CONDIF(label, DNS_PACKET_ ## type (pkt)) \
            CERR(cbor_encode_boolean( &eitem, \
            !! knot_wire_get_ ## name(DNS_PACKET_ ## type (pkt)->knot_packet->wire))); \
        COND_END

    WRITEFLAG(resp_aa, RESPONSE, aa);
    WRITEFLAG(resp_tc, RESPONSE, tc);
    WRITEFLAG(req_rd, REQUEST, rd);
    WRITEFLAG(resp_ra, RESPONSE, ra);
    WRITEFLAG(req_z, REQUEST, z);
    WRITEFLAG(resp_ad, RESPONSE, ad);
    WRITEFLAG(req_cd, REQUEST, cd);

#undef WRITEFLAG

    COND(qname) 
        char qname_buf[1024];
        char *res = knot_dname_to_str(qname_buf, knot_pkt_qname(pkt->knot_packet), sizeof(qname_buf));
        if (res) {
            CERR(cbor_encode_text_stringz(&eitem, res));
        } else {
            CERR(cbor_encode_null(&eitem)); 
        }
    COND_END

    CONDIF(resp_ancount, DNS_PACKET_RESPONSE(pkt)) 
        CERR(cbor_encode_int(&eitem, knot_wire_get_ancount(DNS_PACKET_RESPONSE(pkt)->dns_data)));
    COND_END

    CONDIF(resp_arcount, DNS_PACKET_RESPONSE(pkt))
        CERR(cbor_encode_int(&eitem, knot_wire_get_arcount(DNS_PACKET_RESPONSE(pkt)->dns_data)));
    COND_END

    CONDIF(resp_nscount, DNS_PACKET_RESPONSE(pkt)) 
        CERR(cbor_encode_int(&eitem, knot_wire_get_nscount(DNS_PACKET_RESPONSE(pkt)->dns_data)));
    COND_END

    // EDNS

    const knot_rrset_t *req_opt_rr = (pkt && DNS_PACKET_REQUEST(pkt)) ? DNS_PACKET_REQUEST(pkt)->knot_packet->opt_rr : NULL;
    const knot_rrset_t *resp_opt_rr = (pkt && DNS_PACKET_RESPONSE(pkt)) ? DNS_PACKET_RESPONSE(pkt)->knot_packet->opt_rr : NULL;

    CONDIF(req_edns_ver, req_opt_rr)
        CERR(cbor_encode_int(&eitem, knot_edns_get_version(req_opt_rr)));
    COND_END

    CONDIF(req_edns_udp, req_opt_rr)
        CERR(cbor_encode_int(&eitem, knot_edns_get_payload(req_opt_rr)));
    COND_END

    CONDIF(req_edns_do, req_opt_rr)
        CERR(cbor_encode_boolean(&eitem, !! knot_edns_do(req_opt_rr)));
    COND_END

    CONDIF(resp_edns_rcode, resp_opt_rr)
        CERR(cbor_encode_int(&eitem, knot_edns_get_ext_rcode(resp_opt_rr)));
    COND_END

#define COND_OPT_RR(label, code, rr) \
    CONDIF(label, rr) \
        uint8_t *opt = knot_edns_get_option(rr, code); \
        if (!opt) { CERR(cbor_encode_null(&eitem)); } else { \
            int len = knot_edns_opt_get_length(opt);

#define COND_OPT_RR_END \
    } COND_END

#define WRITE_UNDERSTOOD_LIST(label, code) \
    COND_OPT_RR(label, code, req_opt_rr) \
            CborEncoder elist; \
            CERR(cbor_encoder_create_array(&eitem, &elist, len)); \
            for (int i = 0; i < knot_edns_opt_get_length(opt); i++) { \
                CERR(cbor_encode_int(&elist, ((opt + 2 * sizeof(uint16_t) + i)[i]))); \
            } \
            CERR(cbor_encoder_close_container_checked(&eitem, &elist)); \
    COND_OPT_RR_END

    //COND(req_edns_ping)
        // TODO: distinguish a PING request from DAU,
        // see https://github.com/SIDN/entrada/blob/b787af190267df148683151638ce94508bd6139e/dnslib4java/src/main/java/nl/sidn/dnslib/message/records/edns0/OPTResourceRecord.java#L146
    //COND_END

    WRITE_UNDERSTOOD_LIST(req_edns_dau, 5); // DAU
    WRITE_UNDERSTOOD_LIST(req_edns_dhu, 6); // DHU 
    WRITE_UNDERSTOOD_LIST(req_edns_n3u, 7); // N3U 

    COND_OPT_RR(resp_edns_nsid, 3, resp_opt_rr) // NSID
        CERR(cbor_encode_byte_string(&eitem, opt + 2 * sizeof(uint16_t), len));
    COND_OPT_RR_END

    CONDIF(edns_client_subnet, 0) // TODO: PARSE and WRITE client subnet information
        uint8_t *opt = NULL;
        if (resp_opt_rr)
            opt = knot_edns_get_option(resp_opt_rr, 8); // client subnet from response 
        if (req_opt_rr && !opt)
            opt = knot_edns_get_option(req_opt_rr, 8); // client subnet from request 
        if (opt) {
            CborEncoder elist;
            CERR(cbor_encoder_create_array(&eitem, &elist, CborIndefiniteLength));
            // As in: "[4,'118.71.70',24,0]"
            // Entrada: (fam == 1? "4,": "6,") + address + "/" + sourcenetmask + "," + scopenetmask;
            CERR(cbor_encoder_close_container_checked(&eitem, &elist));
        } else {
            CERR(cbor_encode_null(&eitem));
        }
    COND_END

    CONDIF(edns_other, 0) // TODO: traverse all remaining records
        CborEncoder elist;
        CERR(cbor_encoder_create_array(&eitem, &elist, 4));
        if (req_opt_rr) {
            // ...
        }
        if (resp_opt_rr) {
            // ...
        }
        CERR(cbor_encoder_close_container_checked(&eitem, &elist));
    COND_END

    // Close the array

    CERR(cbor_encoder_close_container_checked(&ebase, &eitem));
    size_t written = cbor_encoder_get_buffer_size(&ebase, outbuf);
    fwrite(outbuf, written, 1, f);

#undef COND
#undef COND_END
#undef CERR

    return written;
}


/**
 * Callback for cbor_output, writes CBOR header.
 */
static void
dns_output_cbor_start_file(struct dns_output *out0, dns_us_time_t UNUSED(time))
{
    struct dns_output_cbor *out = (struct dns_output_cbor *) out0;
    assert(out && out->base.out_file);
    out->base.current_bytes += write_packet(out->base.out_file, out->cbor_fields, NULL);
}


/**
 * Callback for cbor_output, writes singe CBOR record.
 */
static dns_ret_t
dns_output_cbor_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    assert(out0 && pkt);
    struct dns_output_cbor *out = (struct dns_output_cbor *) out0;
    size_t n = write_packet(out->base.out_file, out->cbor_fields, pkt);
    out->base.current_bytes += n;

    // Accounting
    out->base.current_items ++;
    if (!DNS_PACKET_RESPONSE(pkt))
        out->base.current_request_only ++;
    if (!DNS_PACKET_REQUEST(pkt))
        out->base.current_response_only ++;

    return DNS_RET_OK;
}

struct dns_output_cbor *
dns_output_cbor_create(struct dns_config *conf, struct dns_frame_queue *in)
{
    struct dns_output_cbor *out = xmalloc_zero(sizeof(struct dns_output_cbor));

    dns_output_init(&out->base, in, conf->output_path_fmt, conf->output_pipe_cmd, conf->output_period_sec);
    out->base.start_file = dns_output_cbor_start_file;
    out->base.write_packet = dns_output_cbor_write_packet;
    out->base.start_output = dns_output_start;
    out->base.finish_output = dns_output_finish;
    out->base.finalize_output = dns_output_finalize;

    out->cbor_fields = conf->cbor_fields;
    msg(L_INFO, "Selected CBOR fields: %#x", out->cbor_fields);
    return out;
}
