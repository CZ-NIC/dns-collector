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

#include "common.h"
#include "config.h"
#include <ctype.h>

static char *
dns_collector_conf_init(void *data)
{
    struct dns_config *conf = (struct dns_config *) data;

    // Common
    conf->max_frame_duration_sec = 0.5;
    conf->max_frame_size = 1 << 18;
    conf->max_queue_len = 8;
    conf->report_period_sec = 60;

    // Input
    conf->input_uri = "";
    conf->input_filter = "";
    conf->input_snaplen = -1;
    conf->input_promiscuous = 1;
    conf->input_real_time_grace_sec = 0.1; // TODO: allow configuration

    // Packet dump options
    conf->dump_path_fmt = "";
    conf->dump_period_sec = 0;
    conf->dump_compress_level = 4;
    conf->dump_compress_type = 0;
    conf->dump_rate_limit = 0.0;

    // Matching
    conf->match_window_sec = 5.0;

    // General output
    conf->output_type = DNS_OUTPUT_TYPE_CSV;
    conf->output_path_fmt = "";
    conf->output_pipe_cmd = "";
    conf->output_period_sec = 0;

    // CSV output
    conf->csv_separator = ",";
    conf->csv_inline_header = 1;
    conf->csv_external_header_path_fmt = "";
    conf->csv_fields = (1 << dns_of_LAST) - 1; // All fields by default

    // CBOR output
    conf->cbor_fields = (1 << dns_of_LAST) - 1; // All fields by default

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
    switch (conf->output_type) {
        case DNS_OUTPUT_TYPE_CSV:
            if (strlen(conf->csv_separator) != 1)
                return "'csv_separator' needs to be exactly one character";
            if (isalnum(conf->csv_separator[0]) || strchr(".-#_\\\n/", conf->csv_separator[0]) 
                || (conf->csv_separator[0] > 0x7f))
                return "You shold use non-alphanumeric ASCII character for 'csv_separator', avoiding \".-#_\\/\" and newline.";
            if (conf->csv_fields == 0)
                return "'csv_fields' must have at least one field";
            break;
        case DNS_OUTPUT_TYPE_CBOR:
            if (conf->cbor_fields == 0)
                return "'cbor_fields' must have at least one field";
            break;
        default:
            return "only output types 'csv' and 'cbor' currently supported";
    }
    if (conf->dump_compress_level < 0 || conf->dump_compress_level > 9)
        return "'dump_compress_level' must be 0..9";

    return NULL;
}

static const char *dns_output_types[] = {
    "csv", "cbor", NULL };

static const char *dns_dump_compress_types[] = {
    "none",
    "gzip",
    "bz2",
    "lzo",
    "xz",
    NULL};

trace_option_compresstype_t dns_dump_compress_types_num[] = {
    TRACE_OPTION_COMPRESSTYPE_NONE,
    TRACE_OPTION_COMPRESSTYPE_ZLIB,
    TRACE_OPTION_COMPRESSTYPE_BZ2,
    TRACE_OPTION_COMPRESSTYPE_LZO,
    TRACE_OPTION_COMPRESSTYPE_LZMA};

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

        // Packet dump options
        CF_STRING("dump_path_fmt", PTR_TO(struct dns_config, dump_path_fmt)),
        CF_INT("dump_period", PTR_TO(struct dns_config, dump_period_sec)),
        CF_INT("dump_compress_level", PTR_TO(struct dns_config, dump_compress_level)),
        CF_LOOKUP("dump_compress_type", PTR_TO(struct dns_config, dump_compress_type), dns_dump_compress_types),
        CF_DOUBLE("dump_rate_limit", PTR_TO(struct dns_config, dump_rate_limit)),

        // Matching
        CF_DOUBLE("match_window", PTR_TO(struct dns_config, match_window_sec)),

        // General output options
        CF_LOOKUP("output_type", PTR_TO(struct dns_config, output_type), dns_output_types),
        CF_STRING("output_path_fmt", PTR_TO(struct dns_config, output_path_fmt)),
        CF_STRING("output_pipe_cmd", PTR_TO(struct dns_config, output_pipe_cmd)),
        CF_INT("output_period", PTR_TO(struct dns_config, output_period_sec)),

        // CSV output
        CF_STRING("csv_separator", PTR_TO(struct dns_config, csv_separator)),
        CF_INT("csv_inline_header", PTR_TO(struct dns_config, csv_inline_header)),
        CF_STRING("csv_external_header_path_fmt", PTR_TO(struct dns_config, csv_external_header_path_fmt)),
        CF_BITMAP_LOOKUP("csv_fields", PTR_TO(struct dns_config, csv_fields), dns_output_field_flag_names),

        // CBOR output
        CF_BITMAP_LOOKUP("cbor_fields", PTR_TO(struct dns_config, cbor_fields), dns_output_field_flag_names),
        CF_END
    }
};

