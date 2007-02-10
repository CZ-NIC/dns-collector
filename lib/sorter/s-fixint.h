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

static int P(internal)(struct sort_context *ctx, struct sort_bucket *bin, struct sort_bucket *bout, struct sort_bucket *bout_only)
{
  sorter_alloc_buf(ctx);
  struct fastbuf *in = sbuck_read(bin);
  P(key) *buf = ctx->big_buf;
  size_t bufsize = ctx->big_buf_half_size;
#ifdef CPU_64BIT_POINTERS
  bufsize = MIN((u64)bufsize, (u64)~0U * sizeof(P(key)));	// The number of records must fit in uns
#endif
  uns maxkeys = bufsize / sizeof(P(key));

  SORT_XTRACE(3, "s-fixint: Reading (maxkeys=%u)", maxkeys);
  uns n = 0;
  while (n < maxkeys && P(read_key)(in, &buf[n]))
    n++;

  SORT_XTRACE(3, "s-fixint: Sorting %u items", n);
  P(array_sort)(n, buf);

  SORT_XTRACE(3, "s-fixint: Writing");
  struct fastbuf *out = sbuck_write((n < maxkeys) ? bout_only : bout);
  bout->runs++;
  uns merged UNUSED = 0;
  for (uns i=0; i<n; i++)
    {
#ifdef SORT_UNIFY
      if (i < n-1 && !P(compare)(&buf[i], &buf[i+1]))
	{
	  ASSERT(0);			/* FIXME: Implement */
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
  return ctx->big_buf_half_size - 1;	// -1 since if the buffer is full, we don't recognize EOF
}
