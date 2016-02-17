#ifndef DNSCOL_OUTPUT_H
#define DNSCOL_OUTPUT_H

#include <string.h>
#include <ucw/lib.h>
#include <ucw/conf.h>

#include "common.h"
#include "packet.h"

struct dns_output {
    cnode outputs_list;

    dns_ret_t (*write_packet)(struct dns_output *out, dns_packet_t *pkt);
    dns_ret_t (*dump_packet)(struct dns_output *out, dns_packet_t *pkt);
    void (*close)(struct dns_output *out);
    char *path_template;
    double period_sec;
    dns_us_time_t period;
};

char *dns_output_init(struct dns_output *s);

char *dns_output_commit(struct dns_output *s);


extern struct cf_section dns_output_csv_section;
//extern const struct cf_section dns_output_protobuf_section;
//extern const struct cf_section dns_output_dump_section;


#endif /* DNSCOL_OUTPUT_H */
