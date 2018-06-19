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

#include <ucw/clists.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define asort_run ucw_asort_run
#define asort_start_threads ucw_asort_start_threads
#define asort_stop_threads ucw_asort_stop_threads
#define sbuck_drop ucw_sbuck_drop
#define sbuck_has_file ucw_sbuck_has_file
#define sbuck_have ucw_sbuck_have
#define sbuck_new ucw_sbuck_new
#define sbuck_read ucw_sbuck_read
#define sbuck_size ucw_sbuck_size
#define sbuck_swap_out ucw_sbuck_swap_out
#define sbuck_write ucw_sbuck_write
#define sorter_add_radix_bits ucw_sorter_add_radix_bits
#define sorter_alloc ucw_sorter_alloc
#define sorter_alloc_buf ucw_sorter_alloc_buf
#define sorter_bufsize ucw_sorter_bufsize
#define sorter_debug ucw_sorter_debug
#define sorter_fb_params ucw_sorter_fb_params
#define sorter_free_buf ucw_sorter_free_buf
#define sorter_max_multiway_bits ucw_sorter_max_multiway_bits
#define sorter_max_radix_bits ucw_sorter_max_radix_bits
#define sorter_min_multiway_bits ucw_sorter_min_multiway_bits
#define sorter_min_radix_bits ucw_sorter_min_radix_bits
#define sorter_prepare_buf ucw_sorter_prepare_buf
#define sorter_radix_threshold ucw_sorter_radix_threshold
#define sorter_run ucw_sorter_run
#define sorter_small_fb_params ucw_sorter_small_fb_params
#define sorter_small_input ucw_sorter_small_input
#define sorter_stream_bufsize ucw_sorter_stream_bufsize
#define sorter_thread_chunk ucw_sorter_thread_chunk
#define sorter_thread_threshold ucw_sorter_thread_threshold
#define sorter_threads ucw_sorter_threads
#define sorter_trace ucw_sorter_trace
#define sorter_trace_array ucw_sorter_trace_array
#endif

/* Configuration variables */
extern uint sorter_trace, sorter_trace_array, sorter_stream_bufsize;
extern uint sorter_debug, sorter_min_radix_bits, sorter_max_radix_bits, sorter_add_radix_bits;
extern uint sorter_min_multiway_bits, sorter_max_multiway_bits;
extern uint sorter_threads;
extern u64 sorter_bufsize, sorter_small_input;
extern u64 sorter_thread_threshold, sorter_thread_chunk, sorter_radix_threshold;
extern struct fb_params sorter_fb_params, sorter_small_fb_params;

#define SORT_TRACE(x...) do { if (sorter_trace) msg(L_DEBUG, x); } while(0)
#define SORT_XTRACE(level, x...) do { if (sorter_trace >= level) msg(L_DEBUG, x); } while(0)

enum sort_debug {
  SORT_DEBUG_NO_PRESORT = 1,
  SORT_DEBUG_NO_JOIN = 2,
  SORT_DEBUG_KEEP_BUCKETS = 4,
  SORT_DEBUG_NO_RADIX = 8,
  SORT_DEBUG_NO_MULTIWAY = 16,
  SORT_DEBUG_ASORT_NO_RADIX = 32,
  SORT_DEBUG_ASORT_NO_THREADS = 64
};

struct sort_bucket;

struct sort_context {
  struct fastbuf *in_fb;
  struct fastbuf *out_fb;
  uint hash_bits;
  u64 in_size;
  struct fb_params *fb_params;

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

  // Multi-way merge: merge an arbitrary number of source buckets to a single destination bucket.
  void (*multiway_merge)(struct sort_context *ctx, struct sort_bucket **ins, struct sort_bucket *out);

  // Radix split according to hash function
  void (*radix_split)(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket **outs, uint bitpos, uint numbits);

  // State variables of internal_sort
  void *key_buf;
  int more_keys;

  // Timing
  timestamp_t start_time;
  uint last_pass_time;
  uint total_int_time, total_pre_time, total_ext_time;
};

void sorter_run(struct sort_context *ctx);

/* Buffers */

void *sorter_alloc(struct sort_context *ctx, uint size);
void sorter_prepare_buf(struct sort_context *ctx);
void sorter_alloc_buf(struct sort_context *ctx);
void sorter_free_buf(struct sort_context *ctx);

/* Buckets */

struct sort_bucket {
  cnode n;
  struct sort_context *ctx;
  uint flags;
  struct fastbuf *fb;
  byte *filename;
  u64 size;				// Size in bytes (not valid when writing)
  uint runs;				// Number of runs, 0 if not sorted
  uint hash_bits;			// Remaining bits of the hash function
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
ucw_off_t sbuck_size(struct sort_bucket *b);
struct fastbuf *sbuck_read(struct sort_bucket *b);
struct fastbuf *sbuck_write(struct sort_bucket *b);
void sbuck_swap_out(struct sort_bucket *b);

/* Contexts and helper functions for the array sorter */

struct asort_context {
  // Interface between generic code in array.c and functions generated by array.h
  void *array;				// Array to sort
  void *buffer;				// Auxiliary buffer (required when radix-sorting)
  uint num_elts;				// Number of elements in the array
  uint elt_size;				// Bytes per element
  uint hash_bits;			// Remaining bits of the hash function
  uint radix_bits;			// How many bits to process in a single radix-sort pass
  void (*quicksort)(void *array_ptr, uint num_elts);
  void (*quicksplit)(void *array_ptr, uint num_elts, int *leftp, int *rightp);
  void (*radix_count)(void *src_ptr, uint num_elts, uint *cnt, uint shift);
  void (*radix_split)(void *src_ptr, void *dest_ptr, uint num_elts, uint *ptrs, uint shift);

  // Used internally by array.c
  struct rs_work **rs_works;
  struct work_queue *rs_work_queue;
  struct eltpool *eltpool;

  // Configured limits translated from bytes to elements
  uint thread_threshold;
  uint thread_chunk;
  uint radix_threshold;
};

void asort_run(struct asort_context *ctx);
void asort_start_threads(uint run);
void asort_stop_threads(void);

#endif
