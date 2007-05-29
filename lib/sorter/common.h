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
extern uns sorter_debug, sorter_min_radix_bits, sorter_max_radix_bits;
extern u64 sorter_bufsize;
extern struct fb_params sorter_fb_params;

#define SORT_TRACE(x...) do { if (sorter_trace) log(L_DEBUG, x); } while(0)
#define SORT_XTRACE(level, x...) do { if (sorter_trace >= level) log(L_DEBUG, x); } while(0)

enum sort_debug {
  SORT_DEBUG_NO_PRESORT = 1,
  SORT_DEBUG_NO_JOIN = 2,
  SORT_DEBUG_KEEP_BUCKETS = 4,
  SORT_DEBUG_NO_RADIX = 8,
};

struct sort_bucket;

struct sort_context {
  struct fastbuf *in_fb;
  struct fastbuf *out_fb;
  uns hash_bits;
  u64 in_size;

  struct mempool *pool;
  clist bucket_list;
  void *big_buf;
  size_t big_buf_size;

  int (*custom_presort)(struct fastbuf *dest, void *buf, size_t bufsize);

  // Take as much as possible from the source bucket, sort it in memory and dump to destination bucket.
  // Return 1 if there is more data available in the source bucket.
  int (*internal_sort)(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket *out, struct sort_bucket *out_only);

  // Estimate how much input data from `b' will fit in the internal sorting buffer.
  u64 (*internal_estimate)(struct sort_context *ctx, struct sort_bucket *b);

  // Two-way split/merge: merge up to 2 source buckets to up to 2 destination buckets.
  // Bucket arrays are NULL-terminated.
  void (*twoway_merge)(struct sort_context *ctx, struct sort_bucket **ins, struct sort_bucket **outs);

  // Radix split according to hash function
  void (*radix_split)(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket **outs, uns bitpos, uns numbits);

  // State variables of internal_sort
  void *key_buf;
  int more_keys;

  // Timing
  timestamp_t start_time;
  uns last_pass_time;
  uns total_int_time, total_pre_time, total_ext_time;
};

void sorter_run(struct sort_context *ctx);

/* Buffers */

void *sorter_alloc(struct sort_context *ctx, uns size);
void sorter_prepare_buf(struct sort_context *ctx);
void sorter_alloc_buf(struct sort_context *ctx);
void sorter_free_buf(struct sort_context *ctx);

/* Buckets */

struct sort_bucket {
  cnode n;
  struct sort_context *ctx;
  uns flags;
  struct fastbuf *fb;
  byte *filename;
  u64 size;				// Size in bytes (not valid when writing)
  uns runs;				// Number of runs, 0 if not sorted
  uns hash_bits;			// Remaining bits of the hash function
  byte *ident;				// Identifier used in debug messages
};

enum sort_bucket_flags {
  SBF_FINAL = 1,			// This bucket corresponds to the final output file (always 1 run)
  SBF_SOURCE = 2,			// Contains the source file (always 0 runs)
  SBF_CUSTOM_PRESORT = 4,		// Contains source to read via custom presorter
  SBF_OPEN_WRITE = 256,			// We are currently writing to the fastbuf
  SBF_OPEN_READ = 512,			// We are reading from the fastbuf
  SBF_DESTROYED = 1024,			// Already done with, no further references allowed
  SBF_SWAPPED_OUT = 2048,		// Swapped out to a named file
};

struct sort_bucket *sbuck_new(struct sort_context *ctx);
void sbuck_drop(struct sort_bucket *b);
int sbuck_have(struct sort_bucket *b);
int sbuck_has_file(struct sort_bucket *b);
sh_off_t sbuck_size(struct sort_bucket *b);
struct fastbuf *sbuck_read(struct sort_bucket *b);
struct fastbuf *sbuck_write(struct sort_bucket *b);
void sbuck_swap_out(struct sort_bucket *b);
void format_size(byte *buf, u64 x);

#endif
