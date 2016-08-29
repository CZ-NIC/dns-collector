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

#ifndef DNSCOL_DUMP_H
#define DNSCOL_DUMP_H

/**
 * \file dump.h
 * Libtrace packet dumping and rate limiting.
 */

#include <libtrace.h>

#include "common.h"
#include "config.h"
#include "packet.h"

/**
 * Dumping packet trace.
 */
struct dns_dump {

    /** Output rotation period in seconds. Zero or less means do not rotate.
     * The output is rotated whenever sec_since_midnight is divisible by period. */
    int period_sec;

    /** Configured path format string. Owned by the output. */
    char *path_fmt;

    /** Libtrace current dump file name.
     * Owned by the dump, not NULL.*/
    char *uri;

    /** Open trace for dumping. Owned by the dumper.
     * Opens on first packet arrival. */
    libtrace_out_t *trace;

    /** Time of opening of the last output file, or DNS_NO_TIME */
    dns_us_time_t dump_opened;

    /** Output file compression type */
    int compress_type;
    /* Level of compression (1-9, 0 for no compression) */
    int compress_level;

    /** Rate of output token growth */
    double rate;
    /** Current tokens */
    double tokens;
    /** Time of the last packet event */
    dns_us_time_t last_event;

    /** Number of packets dumped in the current file */
    uint64_t current_dumped;
    /** Size of dumped packet bytes as reported by libtrace */
    uint64_t current_bytes;
    /** Number of packets skipped in the current file
     * due to rate limiting */
    uint64_t current_skipped;
};

/**
 * Allocate and initialize the dumper.
 */
struct dns_dump *
dns_dump_create(struct dns_config *conf);

/**
 * Close the trace file. A new one will be opened if another
 * packet arrives.
 */
void
dns_dump_close(struct dns_dump *dump);

/**
 * Open a new trace file.
 */
dns_ret_t
dns_dump_open(struct dns_dump *dump, dns_us_time_t time);

/**
 * Free the dump struct.
 * Call only after dns_dump_finish.
 */
void
dns_dump_destroy(struct dns_dump *dump);

/**
 * Dump a packet (with rate limiting and stats.
 * Possibly rotates the output file.
 */
dns_ret_t
dns_dump_packet(struct dns_dump *dump, libtrace_packet_t *packet, dns_ret_t reason);

#endif /* DNSCOL_DUMP_H */
