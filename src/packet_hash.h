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

#ifndef DNSCOL_HASH_H
#define DNSCOL_HASH_H

#include "common.h"

/**
 * \file packet_hash.h
 * Hashing requests by DNS features for matching.
 */


/**
 * Type for hashed values.
 */
typedef uint64_t dns_hash_value_t;

/**
 * Hash bucket with a linked list of packets with the same key.
 * Every bucket needs to be nonempty (to have data to compare)!
 */
struct dns_packet_hash_bucket {
    /** Hash value before modulo */
    dns_hash_value_t hash_value;
    /** Next bucket with the same hash */
    struct dns_packet_hash_bucket *next;
    /** Location of the pointer to this in the prev bucket or hash table */
    struct dns_packet_hash_bucket **prevp;
    /** Circular list of packets.
     * This is assumed to be nonempty so we can compare primary keys of packets. 
     * The list is sorted by timestamp in ascending order */
    clist packets;
};

#define DNS_PACKET_HASH_MIN_PERCENT 25
#define DNS_PACKET_HASH_BEST_PERCENT 50
#define DNS_PACKET_HASH_MAX_PERCENT 75

/** Maximum number of packets in a hash bucket to search for a matching QNAME
 * (prevents one type of DoS, in normal traffic even 1-2 should suffice) */
#define DNS_PACKET_HASH_MAX_SEARCH 32

/**
 * Hash table structure.
 * Only hashes by primary DNS key (everything but QNAME).
 */
struct dns_packet_hash {
    /** Hash table size and modulo */
    size_t capacity;
    /** Initial and minimal capacity. */
    size_t min_capacity;
    /** Number of buckets in the table */
    size_t buckets;
    /** Seed for hash functions */
    dns_hash_value_t seed;
    /** Hash table itself */
    struct dns_packet_hash_bucket **data;
};

/**
 * Allocate new hash table with given initial (and minimal) capacity. If seed==0, generate a random one.
 */
struct dns_packet_hash *
dns_packet_hash_create(size_t capacity, dns_hash_value_t seed);

/**
 * Free all hash data including the buckets. Does not free the contained packets.
 */
void
dns_packet_hash_destroy(struct dns_packet_hash *h);

/**
 * Insert the packet into the hash, creating a new bucket if necessary
 */
void
dns_packet_hash_insert_packet(struct dns_packet_hash *h, struct dns_packet *p);

/**
 * Remove the packet from the hash, deleting its bucket if empty
 */
void
dns_packet_hash_remove_packet(struct dns_packet_hash *h, struct dns_packet *p);

/**
 * Find a matching request for the given response in the hash, returning the oldest
 * matching request and removing it from the table. Returns NULL when none found.
 */
struct dns_packet *
dns_packet_hash_get_match(struct dns_packet_hash *h, struct dns_packet *p);

/**
* Remove the given packet from the hash, removing the bucket if empty.
* Assumes the packet *is* in the hash!
*/
void
dns_packet_hash_remove_packet(struct dns_packet_hash *h, struct dns_packet *p);

#endif /* DNSCOL_HASH_H */
