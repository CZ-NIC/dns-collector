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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "packet.h"
#include "packet_hash.h"

struct dns_packet_hash *
dns_packet_hash_create(size_t capacity, dns_hash_value_t seed)
{
    struct dns_packet_hash *h = xmalloc_zero(sizeof(struct dns_packet_hash));
    h->capacity = capacity;
    h->min_capacity = capacity;
    h->buckets = 0;
    if (seed == 0) {
        seed = random();
#if RAND_MAX < (1 << 33)
        seed += random() << 32;
#endif
    }
    h->seed = seed;
    h->data = xmalloc_zero(h->capacity * sizeof(struct dns_packet_hash_bucket*));
    return h;
}

void
dns_packet_hash_destroy(struct dns_packet_hash *h)
{
    for (dns_hash_value_t i = 0; i < h->capacity; i++)
        for (struct dns_packet_hash_bucket *b = h->data[i]; b;) {
            struct dns_packet_hash_bucket *t = b->next;
            free(b);
            b = t;
        }
    free(h->data);
    free(h);
}

/**
 * Resize the hashtable to given size (but not smaller than min_capacity). 
 */
static void
dns_packet_hash_resize(struct dns_packet_hash *h, size_t new_capacity)
{
    if (new_capacity < h->min_capacity)
        new_capacity = h->min_capacity;
    if (new_capacity == h->capacity)
        return;

    struct dns_packet_hash_bucket **new_data = xmalloc_zero(new_capacity * sizeof(struct dns_packet_hash_bucket*));
    for (dns_hash_value_t i = 0; i < h->capacity; i++)
        for (struct dns_packet_hash_bucket *b = h->data[i]; b;) {
            struct dns_packet_hash_bucket *t = b->next;
            dns_hash_value_t mod_hash = b->hash_value % new_capacity;
            b->next = new_data[mod_hash];
            b->prevp = &new_data[mod_hash];
            new_data[mod_hash] = b;
            b = t;
        }
    free(h->data);
    h->data = new_data;
    h->capacity = new_capacity;
}

/**
 * Find a bucket for packet p.
 * Returns pointer to pointer to p's bucket in the linked list of buckets,
 * so it is suitable for both bucket inserion and bucket removal.
 * Returns with *ret==NULL when no such bucket (but you can insert it there).
 * Never returns NULL.
 */
static struct dns_packet_hash_bucket **
dns_packet_hash_find_bucket(struct dns_packet_hash *h, struct dns_packet *p, dns_hash_value_t hash_value)
{
    struct dns_packet_hash_bucket **bp;
    for (bp = &h->data[hash_value % h->capacity]; *bp; bp = &((*bp)->next)) {
        struct dns_packet *first_packet = DNS_PACKET_FROM_SECNODE(clist_head(&(*bp)->packets));
        assert(first_packet); // Buckets should never be empty
        if (((*bp)->hash_value == hash_value) && (dns_packet_primary_match(p, first_packet)))
            return bp;
    }
    return bp;
}

void
dns_packet_hash_insert_packet(struct dns_packet_hash *h, struct dns_packet *p)
{
    dns_hash_value_t hash_value = dns_packet_primary_hash(p, h->seed);
    struct dns_packet_hash_bucket **bp = dns_packet_hash_find_bucket(h, p, hash_value);
    struct dns_packet_hash_bucket *b = *bp;

    if (!b) {
        b = xmalloc_zero(sizeof(struct dns_packet_hash_bucket));
        b->hash_value = hash_value;
        clist_init(&b->packets);

        b->next = NULL;
        *bp = b;
        h->buckets ++;
        if (h->buckets > h->capacity * DNS_PACKET_HASH_MAX_PERCENT / 100)
            dns_packet_hash_resize(h, h->buckets * 100 / DNS_PACKET_HASH_BEST_PERCENT);
    }

    clist_add_head(&b->packets, &p->secnode);
}

/**
* Remove the given packet from the given hash bucket, removing the bucket if empty.
* NB: Assumes the packet *is* in the hash and in the bucket.
*/
static void
dns_packet_hash_remove_from_bucket(struct dns_packet_hash *h, struct dns_packet *p, struct dns_packet_hash_bucket **bp)
{
    assert(h && p && bp && (*bp));
    struct dns_packet_hash_bucket *b = *bp;

    clist_remove(&p->secnode);
    if (clist_empty(&b->packets)) {
        *bp = b->next;
        free(b);
        h->buckets --;
        if (h->buckets < h->capacity * DNS_PACKET_HASH_MIN_PERCENT / 100)
            dns_packet_hash_resize(h, h->buckets * 100 / DNS_PACKET_HASH_BEST_PERCENT);
    }      
}

struct dns_packet *
dns_packet_hash_get_match(struct dns_packet_hash *h, struct dns_packet *p, int match_qname)
{
    dns_hash_value_t hash_value = dns_packet_primary_hash(p, h->seed);
    struct dns_packet_hash_bucket **bp = dns_packet_hash_find_bucket(h, p, hash_value);
    if (!*bp)
        return NULL;

    // Search the bucket oldest-to-newest
    int cnt = 0;
    cnode *secnode;
    CLIST_WALK(secnode, (*bp)->packets) {
        struct dns_packet *req = DNS_PACKET_FROM_SECNODE(secnode);
        if ((!match_qname) || dns_packet_qname_match(req, p)) {
            dns_packet_hash_remove_from_bucket(h, req, bp);
            return req;
        }
        // Check for very long lists (a potential DOS vector)
        cnt ++;
        if (cnt > DNS_PACKET_HASH_MAX_SEARCH) {
            char msgbuf[128];
            dns_sockaddr_to_str((struct sockaddr *)&p->dst_addr, msgbuf, sizeof(msgbuf));
            msg(L_WARN, "List of requests from %s (all with the same port and ID) too long (more than %d)",
                msgbuf, DNS_PACKET_HASH_MAX_SEARCH);
            break;
        }
    }
    return NULL;
}

void
dns_packet_hash_remove_packet(struct dns_packet_hash *h, struct dns_packet *p)
{
    dns_hash_value_t hash_value = dns_packet_primary_hash(p, h->seed);
    struct dns_packet_hash_bucket **bp = dns_packet_hash_find_bucket(h, p, hash_value);
    assert(*bp);
    dns_packet_hash_remove_from_bucket(h, p, bp);
}
