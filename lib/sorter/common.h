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

struct sort_bucket {
  cnode n;
  uns flags;
  struct fastbuf *fb;
  byte *name;
  u64 size;				// Size in bytes
  uns runs;				// Number of runs, 0 if unknown
  uns hash_bits;			// Remaining bits of the hash function
  byte *ident;				// Identifier used in debug messages
};

enum sort_bucket_flags {
  SBF_FINAL = 1,			// This bucket corresponds to the final output file
  SBF_SOURCE = 2,			// Contains the source file
};

struct sort_context {
  struct fastbuf *in_fb;
  struct fastbuf *out_fb;
  uns hash_bits;

  struct mempool *pool;
  clist bucket_list;
  byte *big_buf, *big_buf_half;
  uns big_buf_size, big_buf_half_size;

  struct fastbuf *(*custom_presort)(void);
  // Take as much as possible from the source bucket, sort it in memory and dump to destination bucket.
  // Return 1 if there is more data available in the source bucket.
  int (*internal_sort)(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket *out);
  // Two-way split/merge: merge up to 2 source buckets to up to 2 destination buckets.
  // Bucket arrays are NULL-terminated.
  void (*twoway_merge)(struct sort_context *ctx, struct sort_bucket **ins, struct sort_bucket **outs);
};

void sorter_run(struct sort_context *ctx);

struct sort_bucket *sorter_new_bucket(struct sort_context *ctx);
struct fastbuf *sorter_open_read(struct sort_bucket *b);
struct fastbuf *sorter_open_write(struct sort_bucket *b);
void sorter_close_read(struct sort_bucket *b);
void sorter_close_write(struct sort_bucket *b);

#endif
