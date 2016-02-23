#include <time.h>
#include <assert.h>
#include <ucw/lib.h>
#include <ucw/conf.h>

#include "common.h"
#include "output.h"
#include "collector.h"
#include "packet.h"


struct dns_output_pcap {
    struct dns_output base;

    uint32_t dump_reasons;
};

/**
 * Local definition of PCAP packet header to decouple from libpcap and allow compression.
 * From libtrace/lib/libtrace_int.h
 */

struct dns_pcapfile_header {
    uint32_t magic_number;   /* magic number */
    uint16_t version_major;  /* major version number */
    uint16_t version_minor;  /* minor version number */
    int32_t  thiszone;       /* GMT to local correction */
    uint32_t sigfigs;        /* timestamp accuracy */
    uint32_t snaplen;        /* aka "wirelen" */
    uint32_t linktype;        /* data link type */
};

/**
 * Local definition of PCAP file header to decouple from libpcap and allow compression.
 * From libtrace/lib/format_pcapfile.c
 */

struct dns_pcapfile_pkt_header {
    uint32_t ts_sec;      /* Seconds portion of the timestamp */
    uint32_t ts_usec;     /* Microseconds portion of the timestamp */
    uint32_t caplen;      /* Capture length of the packet */
    uint32_t wirelen;     /* The wire length of the packet */
};

/**
 * Callback for pcap_output, writes PCAP header.
 * From libtrace/lib/format_pcapfile.c
 */
void
dns_output_pcap_start_file(struct dns_output *out0, dns_us_time_t time UNUSED)
{
    assert(out0 && out0->col);

    struct dns_output_pcap *out UNUSED = (struct dns_output_pcap *) out0;

    struct dns_pcapfile_header hdr = {
        .magic_number = 0xa1b2c3d4,
        .version_major = 2,
        .version_minor = 4,
        .thiszone = 0,
        .snaplen = out0->col->config->capture_limit,
        .sigfigs = 0,
        .linktype = 101, // LINKTYPE_RAW
    };
    dns_output_write(out0, (void *)&hdr, sizeof(hdr));
}

/**
 * Callback for pcap_output, writes singe dropped packet.
 *
 * NOTE: Writing packet header stolen from pcap_dump() in libpcap/sf-pcap.c to allow write-time compression.
 */
static dns_ret_t
dns_output_pcap_drop_packet(struct dns_output *out0, dns_packet_t *pkt, enum dns_drop_reason reason)
{
    struct dns_output_pcap *out = (struct dns_output_pcap *) out0;

    dns_output_check_rotation(out0, pkt->ts);

    // TODO: check dump (soft/hard) quota?

    if (out->dump_reasons & (1 << reason))
    {
        struct dns_pcapfile_pkt_header sf_hdr = {
            .ts_sec = pkt->ts / 1000000,
            .ts_usec = pkt->ts % 1000000,
            .caplen = pkt->pkt_caplen,
            .wirelen = pkt->pkt_len,
        };
        dns_output_write(out0, (void *)&sf_hdr, sizeof(sf_hdr));
        dns_output_write(out0, (void *)pkt->pkt_data, sf_hdr.caplen);
    }

    return DNS_RET_OK;
}


/**
 * Helper for configuration init.
 */
static char *
dns_output_pcap_conf_init(void *data)
{
    struct dns_output_pcap *out = (struct dns_output_pcap *) data;

    out->base.start_file = dns_output_pcap_start_file;
    out->base.drop_packet = dns_output_pcap_drop_packet;
    out->base.manage_files = 1;

    return dns_output_init(&(out->base));
}


/**
 * Helper for configuration post-processing and validation.
 */
static char *
dns_output_pcap_conf_commit(void *data)
{
    struct dns_output_pcap *out = (struct dns_output_pcap *) data;

    return dns_output_commit(&(out->base));
}


struct cf_section dns_output_pcap_section = {
    CF_TYPE(struct dns_output_pcap),
    .init = &dns_output_pcap_conf_init,
    .commit = &dns_output_pcap_conf_commit,
    CF_ITEMS {
        CF_DNS_OUTPUT_COMMON,
        CF_BITMAP_LOOKUP("dump_reasons", PTR_TO(struct dns_output_pcap, dump_reasons), dns_drop_reason_names),
        CF_END
    }
};

