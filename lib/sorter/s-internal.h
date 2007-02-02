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

static inline void *P(internal_get_data)(P(key) *key)
{
  uns ksize = SORT_KEY_SIZE(*key);
#ifdef SORT_UNIFY
  ksize = ALIGN_TO(ksize, CPU_STRUCT_ALIGN);
#endif
  return (byte *) key + ksize;
}

static int P(internal)(struct sort_context *ctx, struct sort_bucket *bin, struct sort_bucket *bout, struct sort_bucket *bout_only)
{
  sorter_alloc_buf(ctx);
  struct fastbuf *in = sbuck_read(bin);

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
      struct fastbuf *out = sbuck_write(bout); /* FIXME: Using a non-direct buffer would be nice here */
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
  uns merged = 0;
  for (item = item_array; item < last_item; item++)
    {
#ifdef SORT_UNIFY
      if (item < last_item - 1 && !P(compare)(item->key, item[1].key))
	{
	  // Rewrite the item structures with just pointers to keys and place
	  // pointers to data in the secondary array.
	  P(key) **key_array = (void *) item;
	  void **data_array = (void **) ctx->big_buf_half;
	  key_array[0] = item[0].key;
	  data_array[0] = P(internal_get_data)(key_array[0]);
	  uns cnt;
	  for (cnt=1; item+cnt < last_item && !P(compare)(key_array[0], item[cnt].key); cnt++)
	    {
	      key_array[cnt] = item[cnt].key;
	      data_array[cnt] = P(internal_get_data)(key_array[cnt]);
	    }
	  P(write_merged)(out, key_array, data_array, cnt, data_array+cnt);
	  item += cnt - 1;
	  merged += cnt - 1;
	  continue;
	}
#endif
#ifdef SORT_ASSERT_UNIQUE
      ASSERT(item == last_item-1 || P(compare)(item->key, item[1].key) < 0);
#endif
      P(write_key)(out, item->key);
#ifdef SORT_VAR_DATA
      bwrite(out, P(internal_get_data)(item->key), SORT_DATA_SIZE(*item->key));
#endif
    }
#ifdef SORT_UNIFY
  SORT_XTRACE("Merging reduced %d records", merged);
#endif

  return ctx->more_keys;
}
