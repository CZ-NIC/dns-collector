#include "config.h"
#include "output.h"


static char *
dns_collector_conf_init(void *data)
{
    struct dns_config *conf = (struct dns_config *) data;

    conf->capture_limit = 300;
    conf->timeframe_length_sec = 5.0;
    conf->hash_order = 20;

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
        CF_LIST("csv_output", PTR_TO(struct dns_config, outputs), &dns_output_csv_section),
  //    CF_LIST("protobuf_output", &outputs, &dns_output_csv_section),
  //    CF_LIST("dump_output", &outputs, &dns_output_csv_section),
        CF_END
    }
};

