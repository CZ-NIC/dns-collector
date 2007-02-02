/*
 *	UCW Library -- Universal Sorter: Common Declarations
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_SORTER_COMMON_H
#define _UCW_SORTER_COMMON_H

#include "lib/clists.h"

/* Configuration, some of the variables are used by the old sorter, too. */
extern uns sorter_trace, sorter_presort_bufsize, sorter_stream_bufsize;
extern u64 sorter_bufsize;

#define SORT_TRACE(x...) do { if (sorter_trace) log(L_DEBUG, x); } while(0)
#define SORT_XTRACE(x...) do { if (sorter_trace > 1) log(L_DEBUG, x); } while(0)

struct sort_bucket {
  cnode n;
  uns flags;
  struct fastbuf *fb;
  byte *name;
  u64 size;				// Size in bytes
  uns runs;				// Number of runs, 0 if not sorted
  uns hash_bits;			// Remaining bits of the hash function
  byte *ident;				// Identifier used in debug messages
};

enum sort_bucket_flags {
  SBF_FINAL = 1,			// This bucket corresponds to the final output file (always 1 run)
  SBF_SOURCE = 2,			// Contains the source file (always 0 runs)
  SBF_CUSTOM_PRESORT = 4,		// Contains source to read via custom presorter
};

struct sort_context {
  struct fastbuf *in_fb;
  struct fastbuf *out_fb;
  uns hash_bits;

  struct mempool *pool;
  clist bucket_list;
  void *big_buf, *big_buf_half;
  size_t big_buf_size, big_buf_half_size;

  int (*custom_presort)(struct fastbuf *dest, byte *buf, size_t bufsize);
  // Take as much as possible from the source bucket, sort it in memory and dump to destination bucket.
  // Return 1 if there is more data available in the source bucket.
  int (*internal_sort)(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket *out, struct sort_bucket *out_only);
  // Two-way split/merge: merge up to 2 source buckets to up to 2 destination buckets.
  // Bucket arrays are NULL-terminated.
  void (*twoway_merge)(struct sort_context *ctx, struct sort_bucket **ins, struct sort_bucket **outs);

  // State variables of internal_sort
  void *key_buf;
  int more_keys;
};

void sorter_run(struct sort_context *ctx);

void *sorter_alloc(struct sort_context *ctx, uns size);
void sorter_alloc_buf(struct sort_context *ctx);
void sorter_free_buf(struct sort_context *ctx);

// Operations on buckets
struct sort_bucket *sbuck_new(struct sort_context *ctx);
void sbuck_drop(struct sort_bucket *b);
int sbuck_can_read(struct sort_bucket *b);
struct fastbuf *sbuck_open_read(struct sort_bucket *b);
struct fastbuf *sbuck_open_write(struct sort_bucket *b);
void sbuck_close_read(struct sort_bucket *b);
void sbuck_close_write(struct sort_bucket *b);

#endif
