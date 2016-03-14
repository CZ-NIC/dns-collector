#include <assert.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>

#include "common.h"
#include "packet.h"
#include "dns.h"

int32_t
dns_qname_printable(u_char *qname, uint32_t qname_maxlen, char *output, size_t output_len)
{
    assert(qname);
    qname_maxlen = MIN(qname_maxlen, DNS_PACKET_QNAME_MAX_LEN);

    int rem = 0;
    int outpos = 0;

    for(int32_t i = 0; i < qname_maxlen; i++) {
        // read label length
        if (rem == 0) {
            // compressed balel?
            if (qname[i] & 0xc0)
                return -1;
            // final label?
            if (qname[i] == '\0') {
                if (output) {
                    if (outpos >= output_len)
                        return -1;
                    output[outpos++] = '\0';
                }
                return i + 1;
            }
            // intermed. label (skip first dot)
            if (output && (i > 0)) {
                if (outpos >= output_len)
                    return -1;
                output[outpos++] = '.';
            }
            rem = qname[i];
        } else {
            if (((qname[i] >= 'a') && (qname[i] <= 'z')) ||
                ((qname[i] >= 'A') && (qname[i] <= 'Z')) ||
                ((qname[i] >= '0') && (qname[i] <= '9')) ||
                (qname[i] == '-')) {
                if (output) {
                    if (outpos >= output_len)
                        return -1;
                    output[outpos++] = qname[i];
                }
                rem --;
            } else {
                return -1;
            }
        }
    }
    return -1;
}


