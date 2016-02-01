
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <pcap/pcap.h>

#include "timeframe.h"
#include "packet.h"

dns_timeframe_t *
dns_timeframe_create(const struct timespec *time_start, const char *frame_name) 
{
    dns_timeframe_t *frame = (dns_timeframe_t*) calloc(sizeof(dns_timeframe_t), 1);
    if (!frame) return NULL;

    if (time_start) {
        frame->time_start.tv_sec = time_start->tv_sec;
        frame->time_start.tv_nsec = time_start->tv_nsec;
    } else {
        clock_gettime(CLOCK_REALTIME, &(frame->time_start));
    }

    strncpy(frame->name, frame_name, DNSCOL_MAX_FNAME_LEN);
    frame->name[DNSCOL_MAX_FNAME_LEN - 1] = 0;

    return frame;
}

void
dns_timeframe_writeout(dns_timeframe_t *frame)
{
//    if (frame->time_end) 
}




struct queryhash_table;

typedef struct querypair {
    u_char *hash_key;
    uint32_t hash_key_len;
    dns_packet_t *request;
    dns_packet_t *response;
} querypair;

#define HASH_PREFIX(x) queryhash_##x
#define HASH_NODE struct querypair
#define HASH_TABLE_DYNAMIC 

#define HASH_WANT_FIND
#define HASH_WANT_LOOKUP
#define HASH_WANT_CLEANUP

#define HASH_AUTO_POOL 4096
#define HASH_ZERO_FILL
#define HASH_TABLE_ALLOC 

#define HASH_KEY_COMPLEX(x) x hash_key_len, x hash_key
#define HASH_KEY_DECL uint32_t hash_key_len, u_char *hash_key

#define HASH_GIVE_HASHFN
uint HASH_PREFIX(hash)(UNUSED void *t, uint32_t hash_key_len, u_char *hash_key)
{
    return 0; // TODO
}

#define HASH_GIVE_EQ
int HASH_PREFIX(eq)(UNUSED void *t, uint32_t hash_key_len1, u_char *hash_key1, uint32_t hash_key_len2, u_char *hash_key2)
{
    if (hash_key_len1 != hash_key_len2)
        return 0;
    return !memcmp(hash_key1, hash_key2, hash_key_len1);
}

#define HASH_GIVE_INIT_KEY
void HASH_PREFIX(init_key)(UNUSED void *t, querypair *node, uint32_t hash_key_len, u_char *hash_key)
{
    node->hash_key = hash_key;
    node->hash_key_len = hash_key_len;
}

#include <ucw/hashtable.h>





