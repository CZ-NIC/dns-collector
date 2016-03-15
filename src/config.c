#include "config.h"
#include "output.h"
#include "input.h"


static char *
dns_collector_conf_init(void *data)
{
    struct dns_config *conf = (struct dns_config *) data;

    clist_init(&(conf->outputs_csv));
    clist_init(&(conf->outputs_proto));
    clist_init(&(conf->outputs_cbor));

    clist_init(&(conf->outputs));

    conf->capture_limit = 300;
    conf->timeframe_length_sec = 5.0;
    conf->hash_order = 20;
    conf->max_queue_len = 5;
    conf->wait_for_outputs = 0;

    return NULL;
}


static char *
dns_collector_conf_commit(void *data)
{
    struct dns_config *conf = (struct dns_config *) data;

    if ((conf->hash_order  < 0) || (conf->hash_order > 36))
        return "'hash_order' should be 0 .. 36.";

    if (conf->timeframe_length_sec < 0.001)
        return "'timeframe_length' too small, minimum 0.001 sec.";
    conf->timeframe_length = dns_fsec_to_us_time(conf->timeframe_length_sec);

    if (conf->capture_limit < 128)
        return "'capture_limit' too small, minimum 128.";

    clist_insert_list_after(&(conf->outputs_csv), &(conf->outputs.head));
    clist_insert_list_after(&(conf->outputs_proto), &(conf->outputs.head));
    clist_insert_list_after(&(conf->outputs_cbor), &(conf->outputs.head));

    return NULL;
}


struct cf_section dns_config_section = {
    CF_TYPE(struct dns_config),
    CF_INIT(dns_collector_conf_init),
    CF_COMMIT(dns_collector_conf_commit),
    CF_ITEMS {
        CF_INT("capture_limit", PTR_TO(struct dns_config, capture_limit)),
        CF_INT("hash_order", PTR_TO(struct dns_config, hash_order)),
        CF_DOUBLE("timeframe_length", PTR_TO(struct dns_config, timeframe_length_sec)),
        CF_INT("offline", PTR_TO(struct dns_config, wait_for_outputs)),
        #ifdef DNS_WITH_CSV
        CF_LIST("output_csv", PTR_TO(struct dns_config, outputs_csv), &dns_output_csv_section),
        #endif // DNS_WITH_CSV
        #ifdef DNS_WITH_PROTO
        CF_LIST("output_proto", PTR_TO(struct dns_config, outputs_proto), &dns_output_proto_section),
        #endif // DNS_WITH_PROTO
        #ifdef DNS_WITH_CBOR
        CF_LIST("output_cbor", PTR_TO(struct dns_config, outputs_cbor), &dns_output_cbor_section),
        #endif // DNS_WITH_CBOR
        CF_LIST("input", PTR_TO(struct dns_config, inputs), &dns_input_section),
        CF_END
    }
};

