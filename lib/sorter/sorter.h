/*
 *	UCW Library -- Universal Sorter
 *
 *	(c) 2001--2007 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *  This is not a normal header file, but a generator of sorting
 *  routines.  Each time you include it with parameters set in the
 *  corresponding preprocessor macros, it generates a file sorter
 *  with the parameters given.
 *
 *  FIXME: explain the basics (keys and data, callbacks etc.)
 *
 *  Basic parameters and callbacks:
 *
 *  SORT_PREFIX(x)      add a name prefix (used on all global names
 *			defined by the sorter)
 *
 *  SORT_KEY		data type capable of storing a single key.
 *  SORT_KEY_REGULAR	data type holding a single key both in memory and on disk;
 *			in this case, bread() and bwrite() is used to read/write keys
 *			and it's also assumed that the keys are not very long.
 *  int PREFIX_compare(SORT_KEY *a, SORT_KEY *b)
 *			compares two keys, result like strcmp(). Mandatory.
 *  int PREFIX_read_key(struct fastbuf *f, SORT_KEY *k)
 *			reads a key from a fastbuf, returns nonzero=ok, 0=EOF.
 *			Mandatory unless SORT_KEY_REGULAR is defined.
 *  void PREFIX_write_key(struct fastbuf *f, SORT_KEY *k)
 *			writes a key to a fastbuf. Mandatory unless SORT_KEY_REGULAR.
 *
 *  SORT_KEY_SIZE(key)	returns the real size of a key (a SORT_KEY type in memory
 *			can be truncated to this number of bytes without any harm;
 *			used to save memory when the keys have variable sizes).
 *			Default: always store the whole SORT_KEY.
 *  SORT_DATA_SIZE(key)	gets a key and returns the amount of data following it.
 *			Default: records consist of keys only.
 *
 *  Integer sorting:
 *
 *  SORT_INT(key)	We are sorting by an integer value. In this mode, PREFIX_compare
 *			is supplied automatically and the sorting function gets an extra
 *			parameter specifying a range of the integers. The better the range
 *			fits, the faster we sort. Sets up SORT_HASH_xxx automatically.
 *
 *  Hashing (optional, but it can speed sorting up):
 *
 *  SORT_HASH_BITS	signals that a monotone hashing function returning a given number of
 *			bits is available. Monotone hash is a function f such that f(x) < f(y)
 *			implies x < y and which is approximately uniformly distributed.
 *  uns PREFIX_hash(SORT_KEY *a)
 *
 *  Unification:
 *
 *  SORT_UNIFY		merge items with identical keys, needs the following functions:
 *  void PREFIX_write_merged(struct fastbuf *f, SORT_KEY **keys, void **data, uns n, void *buf)
 *			takes n records in memory with keys which compare equal and writes
 *			a single record to the given fastbuf. `buf' points to a buffer which
 *			is guaranteed to hold all given records.
 *  void PREFIX_copy_merged(SORT_KEY **keys, struct fastbuf **data, uns n, struct fastbuf *dest)
 *			takes n records with keys in memory and data in fastbufs and writes
 *			a single record.
 *
 *  Input (choose one of these):
 *
 *  SORT_INPUT_FILE	file of a given name
 *  SORT_INPUT_FB	seekable fastbuf stream
 *  SORT_INPUT_PIPE	non-seekable fastbuf stream
 *  SORT_INPUT_PRESORT	custom presorter. Calls function
 *  int PREFIX_presort(struct fastbuf *dest, void *buf, size_t bufsize);
 *			to get successive batches of pre-sorted data.
 *			The function is passed a page-aligned presorting buffer.
 *			It returns 1 on success or 0 on EOF.
 *
 *  Output (chose one of these):
 *
 *  SORT_OUTPUT_FILE	file of a given name
 *  SORT_OUTPUT_FB	temporary fastbuf stream
 *  SORT_OUTPUT_THIS_FB	a given fastbuf stream which can already contain a header
 *
 *  Other switches:
 *
 *  SORT_UNIQUE		all items have distinct keys (checked in debug mode)
 *
 *  The function generated:
 *
 *  <outfb> PREFIX_SORT(<in>, <out> [,<range>]), where:
 *			<in> = input file name/fastbuf or NULL
 *			<out> = output file name/fastbuf or NULL
 *			<range> = maximum integer value for the SORT_INT mode
 *			<outfb> = output fastbuf (in SORT_OUTPUT_FB mode)
 *
 *  After including this file, all parameter macros are automatically
 *  undef'd.
 */

#include "lib/sorter/common.h"
#include "lib/fastbuf.h"

#include <fcntl.h>

#define P(x) SORT_PREFIX(x)

#ifdef SORT_KEY_REGULAR
typedef SORT_KEY_REGULAR P(key);
static inline int P(read_key) (struct fastbuf *f, P(key) *k)
{
  return breadb(f, k, sizeof(P(key)));
}
static inline void P(write_key) (struct fastbuf *f, P(key) *k)
{
  bwrite(f, k, sizeof(P(key)));
}
#elif defined(SORT_KEY)
typedef SORT_KEY P(key);
#else
#error Missing definition of sorting key.
#endif

#ifdef SORT_INT
static inline int P(compare) (P(key) *x, P(key) *y)
{
  if (SORT_INT(*x) < SORT_INT(*y))
    return -1;
  if (SORT_INT(*x) > SORT_INT(*y))
    return 1;
  return 0;
}

#ifndef SORT_HASH_BITS
static inline int P(hash) (P(key) *x)
{
  return SORT_INT((*x));
}
#endif
#endif

#ifdef SORT_UNIFY
#define LESS <
#else
#define LESS <=
#endif
#define SWAP(x,y,z) do { z=x; x=y; y=z; } while(0)

#if defined(SORT_UNIQUE) && defined(DEBUG_ASSERTS)
#define SORT_ASSERT_UNIQUE
#endif

#ifdef SORT_KEY_SIZE
#define SORT_VAR_KEY
#else
#define SORT_KEY_SIZE(key) sizeof(key)
#endif

#ifdef SORT_DATA_SIZE
#define SORT_VAR_DATA
#else
#define SORT_DATA_SIZE(key) 0
#endif

static inline void P(copy_data)(P(key) *key, struct fastbuf *in, struct fastbuf *out)
{
  P(write_key)(out, key);
#ifdef SORT_VAR_DATA
  bbcopy(in, out, SORT_DATA_SIZE(*key));
#else
  (void) in;
#endif
}

#if defined(SORT_VAR_KEY) || defined(SORT_VAR_DATA) || defined(SORT_UNIFY)
#include "lib/sorter/s-internal.h"
#else
#include "lib/sorter/s-fixint.h"
#endif

#include "lib/sorter/s-twoway.h"

#if defined(SORT_HASH_BITS) || defined(SORT_INT)
#include "lib/sorter/s-radix.h"
#endif

static struct fastbuf *P(sort)(
#ifdef SORT_INPUT_FILE
			       byte *in,
#else
			       struct fastbuf *in,
#endif
#ifdef SORT_OUTPUT_FILE
			       byte *out
#else
			       struct fastbuf *out
#endif
#ifdef SORT_INT
			       , uns int_range
#endif
			       )
{
  struct sort_context ctx;
  bzero(&ctx, sizeof(ctx));

#ifdef SORT_INPUT_FILE
  ctx.in_fb = bopen(in, O_RDONLY, sorter_stream_bufsize);
  ctx.in_size = bfilesize(ctx.in_fb);
#elif defined(SORT_INPUT_FB)
  ctx.in_fb = in;
  ctx.in_size = bfilesize(in);
#elif defined(SORT_INPUT_PIPE)
  ctx.in_fb = in;
  ctx.in_size = ~(u64)0;
#elif defined(SORT_INPUT_PRESORT)
  ASSERT(!in);
  ctx.custom_presort = P(presort);
  ctx.in_size = ~(u64)0;
#else
#error No input given.
#endif

#ifdef SORT_OUTPUT_FB
  ASSERT(!out);
#elif defined(SORT_OUTPUT_THIS_FB)
  ctx.out_fb = out;
#elif defined(SORT_OUTPUT_FILE)
  /* Just assume fastbuf output and rename the fastbuf later */
#else
#error No output given.
#endif

#ifdef SORT_HASH_BITS
  ctx.hash_bits = SORT_HASH_BITS;
  ctx.radix_split = P(radix_split);
#elif defined(SORT_INT)
  ctx.hash_bits = 0;
  while (ctx.hash_bits < 32 && (int_range >> ctx.hash_bits))
    ctx.hash_bits++;
  ctx.radix_split = P(radix_split);
#endif

  ctx.internal_sort = P(internal);
  ctx.internal_estimate = P(internal_estimate);
  ctx.twoway_merge = P(twoway_merge);

  sorter_run(&ctx);

#ifdef SORT_OUTPUT_FILE
  if (rename(ctx.out_fb->name, out) < 0)
    die("Cannot rename %s to %s: %m", ctx.out_fb->name, out);
  bconfig(ctx.out_fb, BCONFIG_IS_TEMP_FILE, 0);
  bclose(ctx.out_fb);
  ctx.out_fb = NULL;
#endif
  return ctx.out_fb;
}

#undef SORT_PREFIX
#undef SORT_KEY
#undef SORT_KEY_REGULAR
#undef SORT_KEY_SIZE
#undef SORT_DATA_SIZE
#undef SORT_VAR_KEY
#undef SORT_VAR_DATA
#undef SORT_INT
#undef SORT_HASH_BITS
#undef SORT_UNIFY
#undef SORT_INPUT_FILE
#undef SORT_INPUT_FB
#undef SORT_INPUT_PRESORT
#undef SORT_OUTPUT_FILE
#undef SORT_OUTPUT_FB
#undef SORT_OUTPUT_THIS_FB
#undef SORT_UNIQUE
#undef SORT_ASSERT_UNIQUE
#undef SWAP
#undef LESS
#undef P
/* FIXME: Check that we undef everything we should. */
