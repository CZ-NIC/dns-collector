#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>

#include "common.h"
#include "dnsquery.pb-c.h"
#include "packet.h"
#include "config.h"

struct dns_output_proto {
    struct dns_output base;

    uint32_t fields;
};

/** Buffer large enough to hold serialised DnsQuery protobuf.
 * All fixed-size attributes should take <=96 bytes even at 8b/attr,
 * IPs and 2xQNAME should fit 4*4+2*16+2*256, 656 bytes total. */
#define DNS_MAX_PROTO_LEN 1024

/**
 * Callback for cvs_output, writes singe DnsQuery protobuf.
 */
//void
//dns_fill_proto(const struct dns_config *conf, const dns_packet_t* request, const dns_packet_t* response, DnsQuery *proto)
//{
    // TODO: Include fields based on config
//}
static dns_ret_t
dns_output_proto_write_packet(struct dns_output *out0, dns_packet_t *pkt)
{
    struct dns_output_proto *out = (struct dns_output_proto *) out0;
    char buf[DNS_MAX_PROTO_LEN + 2];
    uint16_t len;

    const dns_packet_t* request = DNS_PACKET_IS_REQUEST(pkt) ? pkt : NULL;
    const dns_packet_t* response = DNS_PACKET_IS_RESPONSE(pkt) ? pkt : pkt->response;
    if (request && response)
        assert(dns_packets_match(request, response));
    const int addr_len = (pkt->ip_ver == 4 ? 4 : 16);

    DnsQuery proto;
    dns_query__init(&proto);
    
    #define FLG(f) (out->fields & (1 << (dns_of_##f)))

    // flags
    if (FLG(flags)) {
        proto.has_flags = true;
        proto.flags = dns_packet_get_output_flags(pkt);
    }

    // client_addr
    if (FLG(client_addr)) { 
        proto.has_client_addr = true;
        proto.client_addr.len = addr_len;
        if (request)
            proto.client_addr.data = (u_char *)(request->src_addr);
        else
            proto.client_addr.data = (u_char *)(response->dst_addr);
    }

    // client_port
    if (FLG(client_port)) { 
        proto.has_client_port = true;
        if (request)
            proto.client_port = request->src_port;
        else
            proto.client_port = response->dst_port;
    }

    // server_addr
    if (FLG(server_addr)) { 
        proto.has_server_addr = true;
        proto.server_addr.len = addr_len;
        if (request)
            proto.server_addr.data = (u_char *)(request->dst_addr);
        else
            proto.server_addr.data = (u_char *)(response->src_addr);
    }

    // server_port
    if (FLG(server_port)) { 
        proto.has_server_port = true;
        if (request)
            proto.server_port = request->dst_port;
        else
            proto.server_port = response->src_port;
    }

    // id
    if (FLG(id)) { 
        proto.has_id = true;
        proto.id = ntohs(pkt->dns_data->id);
    }
 
    // qname
    if (FLG(qname)) { 
        proto.qname = pkt->dns_qname_string;
    }

    // qtype
    if (FLG(qtype)) { 
        proto.has_qtype = true;
        proto.qtype = dns_packet_get_qtype(pkt);
    }

    // qclass
    if (FLG(qclass)) { 
        proto.has_qclass = true;
        proto.qclass = dns_packet_get_qclass(pkt);
    }

    // request_time_us
    if (FLG(request_time_us) && request) { 
        proto.has_request_time_us = true;
        proto.request_time_us = request->ts;
    }

    // request_flags
    if (FLG(request_flags) && request) { 
        proto.has_request_flags = true;
        proto.request_flags = ntohs(request->dns_data->flags);
    }

    // request_length
    if (FLG(request_length) && request) { 
        proto.has_request_length = true;
        proto.request_length = request->dns_len;
    }

    // response_time_us
    if (FLG(response_time_us) && response) { 
        proto.has_response_time_us = true;
        proto.response_time_us = response->ts;
    }

    // response_flags
    if (FLG(response_flags) && response) { 
        proto.has_response_flags = true;
        proto.response_flags = ntohs(response->dns_data->flags);
    }    

    // response_length
    if (FLG(response_length) && response) { 
        proto.has_response_length = true;
        proto.response_length = response->dns_len;
    }

#undef FLG
    len = protobuf_c_message_pack((ProtobufCMessage *)&proto, (uint8_t *)buf + 2);
    if (len > DNS_MAX_PROTO_LEN) // Should never happen, but defensively:
        die("Impossibly long protobuf (%d, max is %d)", len, DNS_MAX_PROTO_LEN);
    assert(sizeof(len) == 2);
    memcpy(buf, &len, 2);
    
    dns_output_write(out0, buf, len + 2);

    return DNS_RET_OK;
}

/**
 * Helper for configuration init.
 */
static char *
dns_output_proto_conf_init(void *data)
{
    struct dns_output_proto *out = (struct dns_output_proto *) data;

    out->base.write_packet = dns_output_proto_write_packet;
    out->base.manage_files = 1;

    return dns_output_init(&(out->base));
}


/**
 * Helper for configuration post-processing and validation.
 */
static char *
dns_output_proto_conf_commit(void *data)
{
    struct dns_output_proto *out = (struct dns_output_proto *) data;

    return dns_output_commit(&(out->base));
}

struct cf_section dns_output_proto_section = {
    CF_TYPE(struct dns_output_proto),
    .init = &dns_output_proto_conf_init,
    .commit = &dns_output_proto_conf_commit,
    CF_ITEMS {
        CF_STRING("path_template", PTR_TO(struct dns_output, path_template)),
        CF_DOUBLE("period", PTR_TO(struct dns_output, period_sec)),
        CF_BITMAP_LOOKUP("fields", PTR_TO(struct dns_output_proto, fields), dns_output_field_names),
        CF_END
    }
};
 

