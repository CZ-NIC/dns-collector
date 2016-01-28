#include <assert.h>
#include <stdlib.h>

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

