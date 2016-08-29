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

#ifndef DNSCOL_OUTPUT_CSV_H
#define DNSCOL_OUTPUT_CSV_H

/**
 * \file output_csv.h
 * Output to CSV files - configuration and writing.
 */

#include "output.h"
#include "frame_queue.h"
#include "config.h"

/**
 * Configuration structure extending `struct dns_output`.
 */
struct dns_output_csv {
    struct dns_output base;

    /* Separator character */
    int separator;

    /* Write header as the first line of the CSV */
    int inline_header;

    /* Write header into a separate file if not NULL,
     * owned by output */
    char *external_header_path_fmt;

    /* Bitmask with CSV fields to write */
    uint32_t csv_fields;
};

struct dns_output_csv *
dns_output_csv_create(struct dns_config *conf, struct dns_frame_queue *in);

void
dns_output_csv_start(struct dns_output_csv *out);

void
dns_output_csv_finish(struct dns_output_csv *out);

void
dns_output_csv_destroy(struct dns_output_csv *out);


#endif /* DNSCOL_OUTPUT_CSV_H */


