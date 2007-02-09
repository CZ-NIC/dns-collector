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
#include "lib/sorter/common.h"

#include <string.h>

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
  return (out->flags & SBF_FINAL) ? out : NULL;
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
      SORT_XTRACE(2, "%s", ((b->flags & SBF_CUSTOM_PRESORT) ? "Custom presorting" : "Presorting"));
      ins[0] = sbuck_new(ctx);
      if (!sorter_presort(ctx, b, ins[0], join ? : ins[0]))
	{
	  SORT_TRACE("Sorted in memory");
	  if (join)
	    sbuck_drop(ins[0]);
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
      SORT_TRACE("Presorting pass (%d+%d runs, %s+%s)", ins[0]->runs, ins[1]->runs, F_BSIZE(ins[0]), F_BSIZE(ins[1]));
    }
  else
    {
      SORT_XTRACE(2, "Presorting disabled");
      ins[0] = b;
    }

  SORT_XTRACE(2, "Main sorting");
  uns pass = 0;
  do {
    ++pass;
    if (ins[0]->runs == 1 && ins[1]->runs == 1 && join)
      {
	// This is guaranteed to produce a single run, so join if possible
	outs[0] = join;
	outs[1] = NULL;
	ctx->twoway_merge(ctx, ins, outs);
	ASSERT(outs[0]->runs == 2);
	outs[0]->runs--;
	SORT_TRACE("Mergesort pass %d (final run, %s)", pass, F_BSIZE(outs[0]));
	sbuck_drop(ins[0]);
	sbuck_drop(ins[1]);
	return;
      }
    outs[0] = sbuck_new(ctx);
    outs[1] = sbuck_new(ctx);
    outs[2] = NULL;
    ctx->twoway_merge(ctx, ins, outs);
    SORT_TRACE("Mergesort pass %d (%d+%d runs, %s+%s)", pass, outs[0]->runs, outs[1]->runs, F_BSIZE(outs[0]), F_BSIZE(outs[1]));
    sbuck_drop(ins[0]);
    sbuck_drop(ins[1]);
    memcpy(ins, outs, 3*sizeof(struct sort_bucket *));
  } while (sbuck_have(ins[1]));

  sbuck_drop(ins[1]);
  clist_insert_after(&ins[0]->n, list_pos);
}

void
sorter_run(struct sort_context *ctx)
{
  ctx->pool = mp_new(4096);
  clist_init(&ctx->bucket_list);

  /* FIXME: Remember to test sorting of empty files */

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
  SORT_XTRACE(2, "Input size: %s", (ctx->in_size == ~(u64)0 ? (byte*)"unknown" : F_BSIZE(bin)));

  // Create bucket for the output
  struct sort_bucket *bout = sbuck_new(ctx);
  bout->flags = SBF_FINAL;
  if (bout->fb = ctx->out_fb)
    bout->flags |= SBF_OPEN_WRITE;
  bout->ident = "out";
  bout->runs = 1;
  clist_add_head(&ctx->bucket_list, &bout->n);

  struct sort_bucket *b;
  while (bout = clist_head(&ctx->bucket_list), b = clist_next(&ctx->bucket_list, &bout->n))
    {
      if (!sbuck_have(b))
	sbuck_drop(b);
      else if (b->runs == 1)
	sorter_join(b);
      else
	sorter_twoway(ctx, b);
    }

  sorter_free_buf(ctx);
  sbuck_write(bout);		// Force empty bucket to a file
  SORT_XTRACE(2, "Final size: %s", F_BSIZE(bout));
  ctx->out_fb = sbuck_read(bout);
}
