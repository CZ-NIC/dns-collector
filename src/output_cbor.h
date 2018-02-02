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

#ifndef DNSCOL_OUTPUT_CBOR_H
#define DNSCOL_OUTPUT_CBOR_H

/**
 * \file output_cbor.h
 * Output to CBOR files - configuration and writing.
 */

#include "output.h"
#include "frame_queue.h"
#include "config.h"

/**
 * Configuration structure extending `struct dns_output`.
 */
struct dns_output_cbor {
    struct dns_output base;

    /** Fields to write, bitmask of dns_output_field_flag_names */
    uint32_t cbor_fields;
};

struct dns_output_cbor *
dns_output_cbor_create(struct dns_config *conf, struct dns_frame_queue *in);

#endif /* DNSCOL_OUTPUT_CBOR_H */


