/*
 *	Sherlock Library -- Object Buckets
 *
 *	(c) 2001--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_BUCKET_H
#define _SHERLOCK_BUCKET_H

/*
 * Format: The object pool is merely a sequence of object buckets.
 * Each bucket starts with struct obuck_header and it's padded
 * by zeros to a multiple of OBUCK_ALIGN bytes.
 *
 * Locking: Each operation on the pool is protected by a flock.
 *
 * The buckets emulate non-seekable fastbuf streams.
 *
 * fork()'ing if you don't have any bucket open is safe.
 */

extern byte *obuck_name;	/* Internal, for use by buckettool only! */

#define OBUCK_SHIFT 7
#define OBUCK_ALIGN (1<<OBUCK_SHIFT)
#define OBUCK_MAGIC 0xdeadf00d
#define OBUCK_INCOMPLETE_MAGIC 0xdeadfeel
#define OBUCK_TRAILER 0xfeedcafe
#define OBUCK_OID_DELETED (~(oid_t)0)
#define OBUCK_OID_FIRST_SPECIAL (~(oid_t)0xffff)

struct obuck_header {
  u32 magic;			/* OBUCK_MAGIC should dwell here */
  oid_t oid;			/* ID of this object or OBUCK_OID_DELETED */
  u32 length;			/* Length of data in the bucket */
  u32 type;			/* Data type */
  /* Bucket data continue here */
};

enum bucket_type {
  BUCKET_TYPE_COMPAT = 0x7fffffff,	/* and less -- buckets created by older versions of Sherlock */
  BUCKET_TYPE_PLAIN = 0x80000000,	/* plain textual buckets */
  BUCKET_TYPE_V30 = 0x80000001,		/* v3.0 uncompressed buckets */
  BUCKET_TYPE_V30C = 0x80000002		/* v3.0 compressed buckets */
};

struct fastbuf;

void obuck_init(int writeable);	/* Initialize the bucket module */
void obuck_cleanup(void);	/* Clean up the bucket module */
void obuck_sync(void);		/* Flush all buffers to disk */
void obuck_lock_read(void);	/* Explicit locking to make sure other threads don't touch buckets now */
void obuck_lock_write(void);
void obuck_unlock(void);
oid_t obuck_predict_last_oid(void); /* Get OID corresponding to the next to be created bucket (i.e., bucket file size estimate) */

/* Searching for buckets */
void obuck_find_by_oid(struct obuck_header *hdrp);
int obuck_find_first(struct obuck_header *hdrp, int full);
int obuck_find_next(struct obuck_header *hdrp, int full);

/* Reading current bucket */
struct fastbuf *obuck_fetch(void);

/* Creating buckets */
struct fastbuf *obuck_create(u32 type);
void obuck_create_end(struct fastbuf *b, struct obuck_header *hdrp);

/* Deleting buckets */
void obuck_delete(oid_t oid);

/* Fast reading of the whole pool */
struct fastbuf *obuck_slurp_pool(struct obuck_header *hdrp);

/* Convert bucket ID to file position (for size limitations etc.) */

static inline sh_off_t obuck_get_pos(oid_t oid)
{
  return ((sh_off_t) oid) << OBUCK_SHIFT;
}

/* Shaking down bucket file */
void obuck_shakedown(int (*kibitz)(struct obuck_header *old, oid_t new, byte *buck));

#endif
