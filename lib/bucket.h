/*
 *	Sherlock Library -- Object Buckets
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

/*
 * Format: The object pool is merely a sequence of object buckets.
 * Each bucket starts with struct obuck_header and it's padded
 * by zeros to a multiple of OBUCK_ALIGN bytes.
 *
 * Locking: Each operation on the pool is protected by a flock.
 *
 * The buckets emulate non-seekable fastbuf streams.
 */

#define OBUCK_SHIFT 7
#define OBUCK_ALIGN (1<<OBUCK_SHIFT)
#define OBUCK_MAGIC 0xdeadf00d
#define OBUCK_TRAILER 0xfeedcafe
#define OBUCK_OID_DELETED (~(oid_t)0)

struct obuck_header {
  u32 magic;			/* OBUCK_MAGIC should dwell here */
  oid_t oid;			/* ID of this object or OBUCK_OID_DELETED */
  u32 length;			/* Length of compressed data in the bucket */
  u32 orig_length;		/* Length of uncompressed data */
  /* Bucket data continue here */
};

struct fastbuf;

void obuck_init(int writeable);
void obuck_cleanup(void);
struct fastbuf * obuck_fetch(struct obuck_header *hdrp);
void obuck_fetch_abort(struct fastbuf *b);
void obuck_fetch_end(struct fastbuf *b);
struct fastbuf * obuck_write(void);
void obuck_write_end(struct fastbuf *b, struct obuck_header *hdrp);
void obuck_delete(oid_t oid);
struct fastbuf * obuck_walk_init(void);
struct fastbuf * obuck_walk_next(struct fastbuf *b, struct obuck_header *hdrp);
void obuck_walk_end(struct fastbuf *b);
