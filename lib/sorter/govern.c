/*
 *	UCW Library -- Universal Sorter: Governing Routines
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/mempool.h"
#include "lib/stkstring.h"
#include "lib/sorter/common.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#define F_BSIZE(b) stk_fsize(sbuck_size(b))

static void
sorter_start_timer(struct sort_context *ctx)
{
  init_timer(&ctx->start_time);
}

static void
sorter_stop_timer(struct sort_context *ctx, uns *account_to)
{
  ctx->last_pass_time = get_timer(&ctx->start_time);
  *account_to += ctx->last_pass_time;
}

static uns
sorter_speed(struct sort_context *ctx, u64 size)
{
  if (!size)
    return 0;
  if (!ctx->last_pass_time)
    return -1;
  return (uns)((double)size / (1<<20) * 1000 / ctx->last_pass_time);
}

static int
sorter_presort(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket *out, struct sort_bucket *out_only)
{
  sorter_alloc_buf(ctx);
  if (in->flags & SBF_CUSTOM_PRESORT)
    {
      struct fastbuf *f = sbuck_write(out);
      out->runs++;
      return ctx->custom_presort(f, ctx->big_buf, ctx->big_buf_size);	// FIXME: out_only optimization?
    }
  return ctx->internal_sort(ctx, in, out, out_only);
}

static inline struct sort_bucket *
sbuck_join_to(struct sort_bucket *b)
{
  if (sorter_debug & SORT_DEBUG_NO_JOIN)
    return NULL;

  struct sort_bucket *out = (struct sort_bucket *) b->n.prev;	// Such bucket is guaranteed to exist
  if (!(out->flags & SBF_FINAL))
    return NULL;
  ASSERT(out->runs == 1);
  return out;
}

static void
sorter_join(struct sort_bucket *b)
{
  struct sort_bucket *join = (struct sort_bucket *) b->n.prev;
  ASSERT(join->flags & SBF_FINAL);
  ASSERT(b->runs == 1);

  if (!sbuck_has_file(join))
    {
      // The final bucket doesn't have any file associated yet, so replace
      // it with the new bucket.
      SORT_XTRACE(2, "Replaced final bucket");
      b->flags |= SBF_FINAL;
      sbuck_drop(join);
    }
  else
    {
      SORT_TRACE("Copying to output file: %s", F_BSIZE(b));
      struct fastbuf *src = sbuck_read(b);
      struct fastbuf *dest = sbuck_write(join);
      bbcopy(src, dest, ~0U);
      sbuck_drop(b);
    }
}

static void
sorter_twoway(struct sort_context *ctx, struct sort_bucket *b)
{
  struct sort_bucket *ins[3] = { NULL }, *outs[3] = { NULL };
  cnode *list_pos = b->n.prev;
  struct sort_bucket *join = sbuck_join_to(b);

  if (!(sorter_debug & SORT_DEBUG_NO_PRESORT) || (b->flags & SBF_CUSTOM_PRESORT))
    {
      SORT_XTRACE(3, "%s", ((b->flags & SBF_CUSTOM_PRESORT) ? "Custom presorting" : "Presorting"));
      sorter_start_timer(ctx);
      ins[0] = sbuck_new(ctx);
      if (!sorter_presort(ctx, b, ins[0], join ? : ins[0]))
	{
	  sorter_stop_timer(ctx, &ctx->total_pre_time);
	  SORT_XTRACE(((b->flags & SBF_SOURCE) ? 1 : 2), "Sorted in memory");
	  if (join)
	    {
	      ASSERT(join->runs == 2);
	      join->runs--;
	      sbuck_drop(ins[0]);
	    }
	  else
	    clist_insert_after(&ins[0]->n, list_pos);
	  sbuck_drop(b);
	  return;
	}

      ins[1] = sbuck_new(ctx);
      int i = 1;
      while (sorter_presort(ctx, b, ins[i], ins[i]))
	i = 1-i;
      sbuck_drop(b);
      sorter_stop_timer(ctx, &ctx->total_pre_time);
      SORT_TRACE("Presorting pass (%d+%d runs, %s+%s, %dMB/s)",
		 ins[0]->runs, ins[1]->runs,
		 F_BSIZE(ins[0]), F_BSIZE(ins[1]),
		 sorter_speed(ctx, sbuck_size(ins[0]) + sbuck_size(ins[1])));
    }
  else
    {
      SORT_XTRACE(2, "Presorting disabled");
      ins[0] = b;
    }

  SORT_XTRACE(3, "Main sorting");
  uns pass = 0;
  do {
    ++pass;
    sorter_start_timer(ctx);
    if (ins[0]->runs == 1 && ins[1]->runs == 1 && join)
      {
	// This is guaranteed to produce a single run, so join if possible
	sh_off_t join_size = sbuck_size(join);
	outs[0] = join;
	outs[1] = NULL;
	ctx->twoway_merge(ctx, ins, outs);
	ASSERT(join->runs == 2);
	join->runs--;
	join_size = sbuck_size(join) - join_size;
	sorter_stop_timer(ctx, &ctx->total_ext_time);
	SORT_TRACE("Mergesort pass %d (final run, %s, %dMB/s)", pass, stk_fsize(join_size), sorter_speed(ctx, join_size));
	sbuck_drop(ins[0]);
	sbuck_drop(ins[1]);
	return;
      }
    outs[0] = sbuck_new(ctx);
    outs[1] = sbuck_new(ctx);
    outs[2] = NULL;
    ctx->twoway_merge(ctx, ins, outs);
    sorter_stop_timer(ctx, &ctx->total_ext_time);
    SORT_TRACE("Mergesort pass %d (%d+%d runs, %s+%s, %dMB/s)", pass,
	       outs[0]->runs, outs[1]->runs,
	       F_BSIZE(outs[0]), F_BSIZE(outs[1]),
	       sorter_speed(ctx, sbuck_size(outs[0]) + sbuck_size(outs[1])));
    sbuck_drop(ins[0]);
    sbuck_drop(ins[1]);
    memcpy(ins, outs, 3*sizeof(struct sort_bucket *));
  } while (sbuck_have(ins[1]));

  sbuck_drop(ins[1]);
  clist_insert_after(&ins[0]->n, list_pos);
}

static void
sorter_multiway(struct sort_context *ctx, struct sort_bucket *b)
{
  clist parts;
  cnode *list_pos = b->n.prev;
  struct sort_bucket *join = sbuck_join_to(b);
  uns trace_level = (b->flags & SBF_SOURCE) ? 1 : 2;

  clist_init(&parts);
  ASSERT(!(sorter_debug & SORT_DEBUG_NO_PRESORT));
  // FIXME: What if the parts will be too small?
  SORT_XTRACE(3, "%s", ((b->flags & SBF_CUSTOM_PRESORT) ? "Custom presorting" : "Presorting"));
  uns cont;
  uns part_cnt = 0;
  u64 total_size = 0;
  sorter_start_timer(ctx);
  do
    {
      struct sort_bucket *p = sbuck_new(ctx);
      cont = sorter_presort(ctx, b, p, (!part_cnt && join) ? join : p);
      if (sbuck_have(p))
	{
	  part_cnt++;
	  clist_add_tail(&parts, &p->n);
	  total_size += sbuck_size(p);
	  sbuck_swap_out(p);
	}
      else
	sbuck_drop(p);
    }
  while (cont);
  sorter_stop_timer(ctx, &ctx->total_pre_time);
  sbuck_drop(b);

  // FIXME: This is way too similar to the two-way case.
  if (!part_cnt)
    {
      if (join)
	{
          SORT_XTRACE(trace_level, "Sorted in memory and joined");
	  ASSERT(join->runs == 2);
	  join->runs--;
	}
      return;
    }
  if (part_cnt == 1)
    {
      struct sort_bucket *p = clist_head(&parts);
      SORT_XTRACE(trace_level, "Sorted in memory");
      clist_insert_after(&p->n, list_pos);
      return;
    }

  SORT_TRACE("Multi-way presorting pass (%d parts, %s, %dMB/s)", part_cnt, stk_fsize(total_size), sorter_speed(ctx, total_size));

  uns max_ways = 64;
  struct sort_bucket *ways[max_ways+1];
  SORT_XTRACE(2, "Starting up to %d-way merge", max_ways);
  for (;;)
    {
      uns n = 0;
      struct sort_bucket *p;
      while (n < max_ways && (p = clist_head(&parts)))
	{
	  clist_remove(&p->n);
	  ways[n++] = p;
	}
      ways[n] = NULL;
      ASSERT(n > 1);

      struct sort_bucket *out;
      out = sbuck_new(ctx);		// FIXME: No joining so far
      sorter_start_timer(ctx);
      ctx->multiway_merge(ctx, ways, out);
      sorter_stop_timer(ctx, &ctx->total_ext_time);

      for (uns i=0; i<n; i++)
	sbuck_drop(ways[i]);

      if (clist_empty(&parts))
	{
	  clist_insert_after(&out->n, list_pos);
	  SORT_TRACE("Multi-way merge completed (%s, %dMB/s)", F_BSIZE(out), sorter_speed(ctx, sbuck_size(out)));
	  return;
	}
      else
	{
	  sbuck_swap_out(out);
	  clist_add_tail(&parts, &out->n);
	  SORT_TRACE("Multi-way merge pass (%d ways, %s, %dMB/s)", n, F_BSIZE(out), sorter_speed(ctx, sbuck_size(out)));
	}
    }
}

static uns
sorter_radix_bits(struct sort_context *ctx, struct sort_bucket *b)
{
  if (!b->hash_bits || b->hash_bits < sorter_min_radix_bits ||
      !ctx->radix_split ||
      (b->flags & SBF_CUSTOM_PRESORT) ||
      (sorter_debug & SORT_DEBUG_NO_RADIX))
    return 0;

  u64 in = sbuck_size(b);
  u64 mem = ctx->internal_estimate(ctx, b) * 0.8;	// FIXME: Magical factor for hash non-uniformity
  if (in <= mem)
    return 0;

  uns n = sorter_min_radix_bits;
  while (n < sorter_max_radix_bits && n < b->hash_bits && (in >> n) > mem)
    n++;
  return n;
}

static void
sorter_radix(struct sort_context *ctx, struct sort_bucket *b, uns bits)
{
  uns nbuck = 1 << bits;
  SORT_XTRACE(2, "Running radix split on %s with hash %d bits of %d (expecting %s buckets)",
	      F_BSIZE(b), bits, b->hash_bits, stk_fsize(sbuck_size(b) / nbuck));
  sorter_free_buf(ctx);
  sorter_start_timer(ctx);

  struct sort_bucket **outs = alloca(nbuck * sizeof(struct sort_bucket *));
  for (uns i=nbuck; i--; )
    {
      outs[i] = sbuck_new(ctx);
      outs[i]->hash_bits = b->hash_bits - bits;
      clist_insert_after(&outs[i]->n, &b->n);
    }

  ctx->radix_split(ctx, b, outs, b->hash_bits - bits, bits);

  u64 min = ~(u64)0, max = 0, sum = 0;
  for (uns i=0; i<nbuck; i++)
    {
      u64 s = sbuck_size(outs[i]);
      min = MIN(min, s);
      max = MAX(max, s);
      sum += s;
      if (nbuck > 4)
	sbuck_swap_out(outs[i]);
    }

  sorter_stop_timer(ctx, &ctx->total_ext_time);
  SORT_TRACE("Radix split (%d buckets, %s min, %s max, %s avg, %dMB/s)", nbuck,
	     stk_fsize(min), stk_fsize(max), stk_fsize(sum / nbuck), sorter_speed(ctx, sum));
  sbuck_drop(b);
}

void
sorter_run(struct sort_context *ctx)
{
  ctx->pool = mp_new(4096);
  clist_init(&ctx->bucket_list);
  sorter_prepare_buf(ctx);

  // Create bucket containing the source
  struct sort_bucket *bin = sbuck_new(ctx);
  bin->flags = SBF_SOURCE | SBF_OPEN_READ;
  if (ctx->custom_presort)
    bin->flags |= SBF_CUSTOM_PRESORT;
  else
    bin->fb = ctx->in_fb;
  bin->ident = "in";
  bin->size = ctx->in_size;
  bin->hash_bits = ctx->hash_bits;
  clist_add_tail(&ctx->bucket_list, &bin->n);
  SORT_XTRACE(2, "Input size: %s, %d hash bits", F_BSIZE(bin), bin->hash_bits);

  // Create bucket for the output
  struct sort_bucket *bout = sbuck_new(ctx);
  bout->flags = SBF_FINAL;
  if (bout->fb = ctx->out_fb)
    bout->flags |= SBF_OPEN_WRITE;
  bout->ident = "out";
  bout->runs = 1;
  clist_add_head(&ctx->bucket_list, &bout->n);

  struct sort_bucket *b;
  uns bits;
  while (bout = clist_head(&ctx->bucket_list), b = clist_next(&ctx->bucket_list, &bout->n))
    {
      SORT_XTRACE(2, "Next block: %s, %d hash bits", F_BSIZE(b), b->hash_bits);
      if (!sbuck_have(b))
	sbuck_drop(b);
      else if (b->runs == 1)
	sorter_join(b);
      else if (ctx->multiway_merge && !(sorter_debug & (SORT_DEBUG_NO_MULTIWAY | SORT_DEBUG_NO_PRESORT)))	// FIXME: Just kidding...
	sorter_multiway(ctx, b);
      else if (bits = sorter_radix_bits(ctx, b))
	sorter_radix(ctx, b, bits);
      else
	sorter_twoway(ctx, b);
    }

  sorter_free_buf(ctx);
  sbuck_write(bout);		// Force empty bucket to a file
  SORT_XTRACE(2, "Final size: %s", F_BSIZE(bout));
  SORT_XTRACE(2, "Final timings: %.3fs external sorting, %.3fs presorting, %.3fs internal sorting",
	      ctx->total_ext_time/1000., ctx->total_pre_time/1000., ctx->total_int_time/1000.);
  ctx->out_fb = sbuck_read(bout);
}
