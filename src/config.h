/* 
 *  Copyright (C) 2016 CZ.NIC, z.s.p.o.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DNSCOL_COLLECTOR_CONFIG_H
#define DNSCOL_COLLECTOR_CONFIG_H

/**
 * \file config.h
 * DNS collector configuration.
 */

#include <libtrace.h>
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
    double input_real_time_grace_sec;

    // Packet dump options
    char *dump_path_fmt;
    int dump_period_sec;
    int dump_compress_level;
    int dump_compress_type;
    double dump_rate_limit;

    // Matching
    double match_window_sec;

    // General output options
    int output_type;
    char *output_path_fmt;
    char *output_pipe_cmd;
    int output_period_sec;

    // CSV output
    char *csv_separator;
    int csv_inline_header;
    char *csv_external_header_path_fmt;
    uint32_t csv_fields;

    // CBOR output
    uint32_t cbor_fields;
};

extern struct cf_section dns_config_section;

/** TRACE_OPTION_COMPRESSTYPE_ corresponding to the values of dump_compress_type */
extern trace_option_compresstype_t dns_dump_compress_types_num[];

#define DNS_OUTPUT_TYPE_CSV 0
#define DNS_OUTPUT_TYPE_CBOR 1

#endif /* DNSCOL_COLLECTOR_CONFIG_H */
