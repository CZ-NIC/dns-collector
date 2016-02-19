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
    pcap_dumper_t *dumper;
};

/**
 * Callback for pcap_output, writes PCAP header.
 */
void
dns_output_pcap_start_file(struct dns_output *out0, dns_us_time_t time UNUSED)
{
    assert(out0 && out0->col);

    struct dns_output_pcap *out = (struct dns_output_pcap *) out0;
    out->dumper = pcap_dump_fopen(out0->col->pcap, out0->f);
}

/**
 * Callback for pcap_output, writes singe dropped packet.
 */
static dns_ret_t
dns_output_pcap_drop_packet(struct dns_output *out0, dns_packet_t *pkt, enum dns_drop_reason reason)
{
    struct dns_output_pcap *out = (struct dns_output_pcap *) out0;

    dns_output_check_rotation(out0, pkt->ts);

    // TODO: check dump (soft/hard) quota?
    if (out->dump_reasons & (1 << reason))
    {
        const struct pcap_pkthdr hdr = {
            .ts = {.tv_sec = pkt->ts / 1000000, .tv_usec = pkt->ts % 1000000},
            .caplen = pkt->pkt_caplen,
            .len = pkt->pkt_len,
        };
        pcap_dump((u_char *)(out->dumper), &hdr, pkt->pkt_data);
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
        CF_STRING("path_template", PTR_TO(struct dns_output, path_template)),
        CF_DOUBLE("period", PTR_TO(struct dns_output, period_sec)),
        CF_BITMAP_LOOKUP("dump_reasons", PTR_TO(struct dns_output_pcap, dump_reasons), dns_drop_reason_names),
        CF_END
    }
};

