#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>

#include "common.h"
#include "dnsquery.pb-c.h"
#include "writeproto.h"

void
dns_fill_proto(const dns_collector_config_t *conf, const dns_packet_t* request, const dns_packet_t* response, DnsQuery *proto)
{
    // TODO: Include fields based on config
    assert(conf && (request || response) && proto);
    if (request && response)
        assert(dns_packets_match(request, response));
    if (request)
        assert(DNS_HDR_FLAGS_QR(request->dns_data->flags) == 0 && request->dns_data);
    if (response)
        assert(DNS_HDR_FLAGS_QR(response->dns_data->flags) == 1 && response->dns_data);

    dns_query__init(proto);
    const dns_packet_t* const any = (request ? request : response);
    const int addr_len = (any->ip_ver == 4 ? 4 : 16);

    // flags
    if (1) {
        proto->has_flags = true;
        if (any->ip_ver == 6)
            proto->flags |= DNS_QUERY__FLAGS__PRTOCOL_IPV6;
        if (any->ip_proto == IPPROTO_TCP)
            proto->flags |= DNS_QUERY__FLAGS__PROTOCOL_TCP;
        if (request) 
            proto->flags |= DNS_QUERY__FLAGS__HAS_REQUEST;
        if (response) 
            proto->flags |= DNS_QUERY__FLAGS__HAS_RESPONSE;
    }

    // client_addr
    if (1) { 
        proto->has_client_addr = true;
        proto->client_addr.len = addr_len;
        if (request)
            proto->client_addr.data = (u_char *)(request->src_addr);
        else
            proto->client_addr.data = (u_char *)(response->dst_addr);
    }

    // client_port
    if (1) { 
        proto->has_client_port = true;
        if (request)
            proto->client_port = request->src_port;
        else
            proto->client_port = response->dst_port;
    }

    // server_addr
    if (0) { 
        proto->has_server_addr = true;
        proto->server_addr.len = addr_len;
        if (request)
            proto->server_addr.data = (u_char *)(request->dst_addr);
        else
            proto->server_addr.data = (u_char *)(response->src_addr);
    }

    // server_port
    if (0) { 
        proto->has_server_port = true;
        if (request)
            proto->server_port = request->dst_port;
        else
            proto->server_port = response->src_port;
    }

    // id
    if (1) { 
        proto->has_id = true;
        proto->id = ntohs(any->dns_data->id);
    }
 
    // qname_raw
    if (0) { 
        proto->has_qname_raw = true;
        proto->qname_raw.len = any->dns_qname_raw_len;
        proto->qname_raw.data = any->dns_qname_raw;
    }

    // qname
    if (1 && any->dns_qname_string) { 
        proto->qname = any->dns_qname_string;
    }

    // qtype
    if (1) { 
        proto->has_qtype = true;
        proto->qtype = dns_packet_get_qtype(any);
    }

    // qclass
    if (1) { 
        proto->has_qclass = true;
        proto->qclass = dns_packet_get_qclass(any);
    }

    // request_time_us
    if (1 && request) { 
        proto->has_request_time_us = true;
        proto->request_time_us = request->ts;
    }

    // request_flags
    if (1 && request) { 
        proto->has_request_flags = true;
        proto->request_flags = ntohs(request->dns_data->flags);
    }

    // request_length
    if (1 && request) { 
        proto->has_request_length = true;
        proto->request_length = request->dns_len;
    }

    // response_time_us
    if (1 && response) { 
        proto->has_response_time_us = true;
        proto->response_time_us = response->ts;
    }

    // response_flags
    if (1 && response) { 
        proto->has_response_flags = true;
        proto->response_flags = ntohs(response->dns_data->flags);
    }    

    // response_length
    if (1 && response) { 
        proto->has_response_length = true;
        proto->response_length = response->dns_len;
    }
}

