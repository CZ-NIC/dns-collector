
#include <stdint.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <pcap/pcap.h>

#include "common.h"
#include "packet.h"

void
dns_drop_packet(dns_collector *col, dns_packet_t* pkt, dns_drop_reason reason)
{
    assert(col && pkt && pkt->pkt_header && pkt->pkt_data && reason < dns_drop_LAST);

    col->stats.packets_dropped++;
    col->stats.packets_dropped_reason[reason]++;
    // TODO: frame stats

    if (col->config->dump_packet_reason[reason])
    {
        // TODO: check dump (soft/hard) quota
        if (col->pcap_dump) {
            pcap_dump((u_char *)(col->pcap_dump), pkt->pkt_header, pkt->pkt_data);
            col->stats.packets_dumped++;
            col->stats.packets_dumped_reason[reason]++;
        }
    }
}

dns_ret
dns_parse_packet(dns_collector *col, dns_packet_t* pkt, struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
    assert(col && pkt && pkt_header && pkt_data);

    // basic size assertions


}

