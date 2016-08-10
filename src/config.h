#ifndef DNSCOL_COLLECTOR_CONFIG_H
#define DNSCOL_COLLECTOR_CONFIG_H

/**
 * \file config.h
 * DNS collector configuration.
 */

#include "common.h"

/**
 * Configuration of a DNS collector. Filled in mostly by the `libucw` config system.
 * See `dnscol.conf` for detaled description.
 */
struct dns_config {

    // Common
    double max_frame_duration_sec;
    int max_frame_size;
    int max_queue_len;
    int report_period_sec;

    // Input
    char *input_uri;
    char *input_filter;
    int input_snaplen;
    int input_promiscuous;

    // Matching
    double match_window_sec;

    // General output options
    char *output_type;
    char *output_path_fmt;
    char *output_pipe_cmd;
    int output_period_sec;

    // CSV output
    char *csv_separator;
    int csv_inline_header;
    char *csv_external_header_path_fmt;
    uint32_t csv_fields;
};

extern struct cf_section dns_config_section;

#endif /* DNSCOL_COLLECTOR_CONFIG_H */
