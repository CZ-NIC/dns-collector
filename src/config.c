#include "common.h"
#include "config.h"

static char *
dns_collector_conf_init(void *data)
{
    struct dns_config *conf = (struct dns_config *) data;

    // Common
    conf->max_frame_duration_sec = 0.5;
    conf->max_frame_size = 1 << 18;
    conf->max_queue_len = 8;
    conf->report_period_sec = 5;

    // Input
    conf->input_uri = "";
    conf->input_filter = "";
    conf->input_snaplen = 0;
    conf->input_promiscuous = 1;

    // Matching
    conf->match_window_sec = 30.0;

    // conf->output options
    conf->output_type = "csv";
    conf->output_path_fmt = "";
    conf->output_pipe_cmd = "";
    conf->output_period_sec = 300;

    // conf->output
    conf->csv_separator = ",";
    conf->csv_inline_header = 1;
    conf->csv_external_header_path_fmt = "";
    conf->csv_fields = ;

    return NULL;
}


static char *
dns_collector_conf_commit(void *data)
{
    struct dns_config *conf = (struct dns_config *) data;

    if (conf->max_frame_duration_sec < 0.001)
        return "'max_frame_duration_sec' too small, minimum 0.001 sec";
    if (conf->max_queue_len < 1)
        return "'max_queue_len' must be at least 1";
    if (strcasecmp(conf->output_type, "csv") == 0) {
        if (strlen(conf->csv_separator) != 1)
            return "'csv_separator' needs to be exactly one character";
        if ((conf->csv_separator[0] < 0x20) || (conf->csv_separator[0] > 0x7f))
            return "'csv_separator' should be a printable, non-whitespace character";
        if (conf->csv_fields == 0)
            return "'csv_fields' must have at least one field";
    } else {
        return "only 'output_type csv' currently supported";
    }

    return NULL;
}


struct cf_section dns_config_section = {
    CF_TYPE(struct dns_config),
    CF_INIT(dns_collector_conf_init),
    CF_COMMIT(dns_collector_conf_commit),
    CF_ITEMS {
	// Common
	CF_DOUBLE("max_frame_duration", PTR_TO(struct dns_config, max_frame_duration_sec)),
	CF_INT("max_frame_size", PTR_TO(struct dns_config, max_frame_size)),
	CF_INT("max_queue_len", PTR_TO(struct dns_config, max_queue_len)),
	CF_INT("report_period", PTR_TO(struct dns_config, report_period_sec)),

	// Input
	CF_STRING("input_uri", PTR_TO(struct dns_config, input_uri)),
	CF_STRING("input_filter", PTR_TO(struct dns_config, input_filter)),
	CF_INT("input_snaplen", PTR_TO(struct dns_config, input_snaplen)),
	CF_INT("input_promiscuous", PTR_TO(struct dns_config, input_promiscuous)),

	// Matching
	CF_DOUBLE("match_window", PTR_TO(struct dns_config, match_window_sec)),

	// General output options
	CF_STRING("output_type", PTR_TO(struct dns_config, output_type)),
	CF_STRING("output_path_fmt", PTR_TO(struct dns_config, output_path_fmt)),
	CF_STRING("output_pipe_cmd", PTR_TO(struct dns_config, output_pipe_cmd)),
	CF_INT("output_period", PTR_TO(struct dns_config, output_period_sec)),

	// CSV output
	CF_STRING("csv_separator", PTR_TO(struct dns_config, csv_separator)),
	CF_INT("csv_inline_header", PTR_TO(struct dns_config, csv_inline_header)),
	CF_STRING("csv_external_header_path_fmt", PTR_TO(struct dns_config, csv_external_header_path_fmt)),
	CF_BITMAP_LOOKUP("csv_fields", PTR_TO(struct dns_config, csv_fields), dns_output_field_names),
        CF_END
    }
};

