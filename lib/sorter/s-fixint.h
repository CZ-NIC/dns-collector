/*
 *	UCW Library -- Universal Sorter: Fixed-Size Internal Sorting Module
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define ASORT_PREFIX(x) SORT_PREFIX(array_##x)
#define ASORT_KEY_TYPE P(key)
#define ASORT_ELT(i) ary[i]
#define ASORT_LT(x,y) (P(compare)(&(x), &(y)) < 0)
#define ASORT_EXTRA_ARGS , P(key) *ary
#include "lib/arraysort.h"

static int P(internal_num_keys)(struct sort_context *ctx)
{
  size_t bufsize = ctx->big_buf_half_size;
#ifdef SORT_UNIFY
  // When we promise unification, we have to reduce the number of records
  // to be sure that both pointers and merged records fit in the 2nd half
  // of the big_buf. So we eat as much memory as s-internal.h, but still
  // we are faster.
  u64 maxkeys = bufsize / (sizeof(P(key)) + sizeof(void *));
#else
  u64 maxkeys = bufsize / sizeof(P(key));
#endif
  return MIN(maxkeys, ~0U);					// The number of records must fit in uns
}

static int P(internal)(struct sort_context *ctx, struct sort_bucket *bin, struct sort_bucket *bout, struct sort_bucket *bout_only)
{
  sorter_alloc_buf(ctx);
  struct fastbuf *in = sbuck_read(bin);
  P(key) *buf = ctx->big_buf;
  uns maxkeys = P(internal_num_keys)(ctx);

  SORT_XTRACE(3, "s-fixint: Reading (maxkeys=%u)", maxkeys);
  uns n = 0;
  while (n < maxkeys && P(read_key)(in, &buf[n]))
    n++;
  if (!n)
    return 0;

  SORT_XTRACE(3, "s-fixint: Sorting %u items", n);
  timestamp_t timer;
  init_timer(&timer);
  P(array_sort)(n, buf);
  ctx->total_int_time += get_timer(&timer);

  SORT_XTRACE(3, "s-fixint: Writing");
  if (n < maxkeys)
    bout = bout_only;
  struct fastbuf *out = sbuck_write(bout);
  bout->runs++;
  uns merged UNUSED = 0;
  for (uns i=0; i<n; i++)
    {
#ifdef SORT_UNIFY
      if (i < n-1 && !P(compare)(&buf[i], &buf[i+1]))
	{
	  P(key) **keys = ctx->big_buf_half;
	  uns n = 2;
	  keys[0] = &buf[i];
	  keys[1] = &buf[i+1];
	  while (!P(compare)(&buf[i], &buf[i+n]))
	    {
	      keys[n] = &buf[i+n];
	      n++;
	    }
	  P(write_merged)(out, keys, NULL, n, keys+n);
	  merged += n - 1;
	  i += n - 1;
	  continue;
	}
#endif
#ifdef SORT_ASSERT_UNIQUE
      ASSERT(i == n-1 || P(compare)(&buf[i], &buf[i+1]) < 0);
#endif
      P(write_key)(out, &buf[i]);
    }
#ifdef SORT_UNIFY
  SORT_XTRACE(3, "Merging reduced %d records", merged);
#endif

  return (n == maxkeys);
}

static u64
P(internal_estimate)(struct sort_context *ctx, struct sort_bucket *b UNUSED)
{
  return P(internal_num_keys)(ctx) * sizeof(P(key)) - 1;	// -1 since if the buffer is full, we don't recognize EOF
}
