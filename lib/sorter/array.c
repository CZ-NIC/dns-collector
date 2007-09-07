/*
 *	UCW Library -- Optimized Array Sorter
 *
 *	(c) 2003--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/sorter/common.h"

#include <string.h>

#define ASORT_MIN_RADIX 5000		// FIXME: var?
#define ASORT_MIN_SHIFT 2

static void
asort_radix(struct asort_context *ctx, void *array, void *buffer, uns num_elts, uns hash_bits, uns swapped_output)
{
  uns buckets = (1 << ctx->radix_bits);
  uns shift = (hash_bits > ctx->radix_bits) ? (hash_bits - ctx->radix_bits) : 0;
  uns cnt[buckets];

#if 0
  static int reported[64];
  if (!reported[hash_bits]++)
#endif
  DBG(">>> n=%d h=%d s=%d sw=%d", num_elts, hash_bits, shift, swapped_output);

  bzero(cnt, sizeof(cnt));
  ctx->radix_count(array, num_elts, cnt, shift);

  uns pos = 0;
  for (uns i=0; i<buckets; i++)
    {
      uns j = cnt[i];
      cnt[i] = pos;
      pos += j;
    }
  ASSERT(pos == num_elts);

  ctx->radix_split(array, buffer, num_elts, cnt, shift);
  pos = 0;
  for (uns i=0; i<buckets; i++)
    {
      uns n = cnt[i] - pos;
      if (n < ASORT_MIN_RADIX || shift < ASORT_MIN_SHIFT)
	{
	  ctx->quicksort(buffer, n);
	  if (!swapped_output)
	    memcpy(array, buffer, n * ctx->elt_size);
	}
      else
	asort_radix(ctx, buffer, array, n, shift, !swapped_output);
      array += n * ctx->elt_size;
      buffer += n * ctx->elt_size;
      pos = cnt[i];
    }
}

#ifdef CONFIG_UCW_THREADS

#endif

void
asort_run(struct asort_context *ctx)
{
  SORT_XTRACE(10, "Array-sorting %d items per %d bytes, hash_bits=%d", ctx->num_elts, ctx->elt_size, ctx->hash_bits);

  if (ctx->num_elts < ASORT_MIN_RADIX ||
      ctx->hash_bits <= ASORT_MIN_SHIFT ||
      !ctx->radix_split ||
      (sorter_debug & SORT_DEBUG_ASORT_NO_RADIX))
    {
      SORT_XTRACE(12, "Decided to use direct quicksort");
      ctx->quicksort(ctx->array, ctx->num_elts);
    }
  else
    {
      SORT_XTRACE(12, "Decided to use radix-sort");
      // FIXME: select dest buffer
      asort_radix(ctx, ctx->array, ctx->buffer, ctx->num_elts, ctx->hash_bits, 0);
    }

  SORT_XTRACE(11, "Array-sort finished");
}
