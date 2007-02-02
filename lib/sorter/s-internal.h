/*
 *	UCW Library -- Universal Sorter: Internal Sorting Module
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

typedef struct {
  P(key) *key;
  // FIXME: Add the hash here to save cache misses
} P(internal_item_t);

#define ASORT_PREFIX(x) SORT_PREFIX(array_##x)
#define ASORT_KEY_TYPE P(internal_item_t)
#define ASORT_ELT(i) ary[i]
#define ASORT_LT(x,y) (P(compare)((x).key, (y).key) < 0)
#define ASORT_EXTRA_ARGS , P(internal_item_t) *ary
#include "lib/arraysort.h"

static int P(internal)(struct sort_context *ctx, struct sort_bucket *bin, struct sort_bucket *bout, struct sort_bucket *bout_only)
{
  sorter_alloc_buf(ctx);
  ASSERT(bin->fb);			// Expects the input bucket to be already open for reading
  struct fastbuf *in = bin->fb;

  P(key) key, *keybuf = ctx->key_buf;
  if (!keybuf)
    keybuf = ctx->key_buf = sorter_alloc(ctx, sizeof(key));
  if (ctx->more_keys)
    {
      key = *keybuf;
      ctx->more_keys = 0;
    }
  else if (!P(read_key)(in, &key))
    return 0;

#ifdef SORT_VAR_DATA
  if (sizeof(key) + 1024 + SORT_DATA_SIZE(key) > ctx->big_buf_half_size)
    {
      SORT_XTRACE("s-internal: Generating a giant run");
      struct fastbuf *out = sorter_open_write(bout); /* FIXME: Using a non-direct buffer would be nice here */
      P(copy_data)(&key, in, out);
      bout->runs++;
      return 1;				// We don't know, but 1 is always safe
    }
#endif

  size_t bufsize = ctx->big_buf_half_size;	/* FIXME: In some cases, we can use the whole buffer */
  bufsize = MIN((u64)bufsize, (u64)~0U * sizeof(P(internal_item_t)));	// The number of records must fit in uns

  SORT_XTRACE("s-internal: Reading (bufsize=%zd)", bufsize);
  P(internal_item_t) *item_array = ctx->big_buf, *item = item_array, *last_item;
  byte *end = (byte *) ctx->big_buf + bufsize;
  do
    {
      uns ksize = SORT_KEY_SIZE(key);
#ifdef SORT_UNIFY
      uns ksize_aligned = ALIGN_TO(ksize, CPU_STRUCT_ALIGN);
#else
      uns ksize_aligned = ksize;
#endif
      uns dsize = SORT_DATA_SIZE(key);
      uns recsize = ALIGN_TO(ksize_aligned + dsize, CPU_STRUCT_ALIGN);
      if (unlikely(sizeof(P(internal_item_t)) + recsize > (size_t)(end - (byte *) item)))
	{
	  ctx->more_keys = 1;
	  *keybuf = key;
	  break;
	}
      end -= recsize;
      memcpy(end, &key, ksize);
#ifdef SORT_VAR_DATA
      breadb(in, end + ksize_aligned, dsize);
#endif
      item->key = (P(key)*) end;
      item++;
    }
  while (P(read_key)(in, &key));
  last_item = item;

  uns count = last_item - item_array;
  SORT_XTRACE("s-internal: Sorting %d items", count);
  P(array_sort)(count, item_array);

  SORT_XTRACE("s-internal: Writing");
  if (!ctx->more_keys)
    bout = bout_only;
  struct fastbuf *out = sbuck_write(bout);
  bout->runs++;
  /* FIXME: No unification done yet */
  for (item = item_array; item < last_item; item++)
    {
      P(write_key)(out, item->key);
#ifdef SORT_VAR_DATA
      uns ksize = SORT_KEY_SIZE(*item->key);
#ifdef SORT_UNIFY
      ksize = ALIGN_TO(ksize, CPU_STRUCT_ALIGN);
#endif
      bwrite(out, (byte *) item->key + ksize, SORT_DATA_SIZE(*item->key));
#endif
    }

  return ctx->more_keys;
}
