#include <assert.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>

#include "common.h"
#include "dns.h"

int32_t
dns_query_check(u_char *query, uint32_t caplen)
{
    assert(query);

    int rem = 0;
    for(int32_t i = 0; i < caplen; i++) {
        if (rem == 0) {
            if (query[i] == '\0')
                return i + 1;
            if (query[i] & 0xc0)
                return -1;
            rem = query[i];
        } else {
            if (query[i] == '\0')
                return -1;
            rem --;
        }
    }
    return -1;
}

int
dns_query_to_printable(u_char *query, char *output)
{
    assert(query && output);

    int rem = 0;
    int bad = 0;

    // first label: do not start output with a dot
    if (*query == '\0') {
        *output = '\0';
        return bad;
    } else {
        assert((*query & 0xc0) == 0);
        rem = *query;
        query++;
    }

    for(;; query++, output++) {
        if (rem == 0) {
            if (*query == '\0')
            {
                *output = '\0';
                return bad;
            }
            assert((*query & 0xc0) == 0);
            *output = '.';
            rem = *query;
        } else {
            if (((*query >= 'a') && (*query <= 'z')) ||
                ((*query >= 'A') && (*query <= 'Z')) ||
                ((*query >= '0') && (*query <= '9')) ||
                (*query == '-')) {
                *output = *query;
            } else {
                bad++;
                *output = '#';
            }
            rem --;
        }
    }
}

uint16_t
dns_hdr_flags(const struct dns_hdr *hdr)
{ 
    return ntohs(((const uint16_t*)hdr)[1]);
}

