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
    uint8_t outbuf[2048];
    CborEncoder ebase, eitem;

// Run cmd and die (with an error meaasge) on any CBOR error
#define CERR(cmd) { CborError cerr__ = (cmd); \
        if (cerr__ != CborNoError) { die("CBOR error: %s", cbor_error_string(cerr__)); } }

    cbor_encoder_init(&ebase, outbuf, sizeof(outbuf), 0);
    CERR(cbor_encoder_create_array(&ebase, &eitem, 1));

// Start writing a field conditional on the field flag being set,
// write just the field name for the header if pkt==NULL
#define COND(fieldname) \
        if (fields & (1 << (dns_field_ ## fieldname))) { \
            if (!pkt) { CERR(cbor_encode_text_stringz(&eitem, #fieldname )); } else { \

// Closing for COND(...) statement
#define COND_END  } }

    // Time

    COND(time)
        CERR(cbor_encode_double(&eitem, (double)(pkt->ts) / 1000000.0));
    COND_END

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
