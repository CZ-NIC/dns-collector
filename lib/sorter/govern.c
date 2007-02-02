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

void *
sorter_alloc(struct sort_context *ctx, uns size)
{
  return mp_alloc_zero(ctx->pool, size);
}

struct sort_bucket *
sbuck_new(struct sort_context *ctx)
{
  return sorter_alloc(ctx, sizeof(struct sort_bucket));
}

void
sbuck_drop(struct sort_bucket *b)
{
  if (b)
    {
      if (b->n.prev)
	clist_remove(&b->n);
      bclose(b->fb);
      bzero(b, sizeof(*b));
    }
}

int
sbuck_can_read(struct sort_bucket *b)
{
  return b && b->size;
}

struct fastbuf *
sbuck_open_read(struct sort_bucket *b)
{
  /* FIXME: These functions should handle buckets with no fb and only name. */
  ASSERT(b->fb);
  return b->fb;
}

struct fastbuf *
sbuck_open_write(struct sort_bucket *b)
{
  if (!b->fb)
    b->fb = bopen_tmp(sorter_stream_bufsize);
  return b->fb;
}

void
sbuck_close_read(struct sort_bucket *b)
{
  if (!b)
    return;
  ASSERT(b->fb);
  bclose(b->fb);
  b->fb = NULL;
}

void
sbuck_close_write(struct sort_bucket *b)
{
  if (b->fb)
    {
      b->size = btell(b->fb);
      brewind(b->fb);
    }
}

void
sorter_alloc_buf(struct sort_context *ctx)
{
  if (ctx->big_buf)
    return;
  u64 bs = MAX(sorter_bufsize/2, 1);
  bs = ALIGN_TO(bs, (u64)CPU_PAGE_SIZE);
  ctx->big_buf = big_alloc(2*bs);
  ctx->big_buf_size = 2*bs;
  ctx->big_buf_half = ((byte*) ctx->big_buf) + bs;
  ctx->big_buf_half_size = bs;
  SORT_XTRACE("Allocated sorting buffer (%jd bytes)", (uintmax_t) bs);
}

void
sorter_free_buf(struct sort_context *ctx)
{
  if (!ctx->big_buf)
    return;
  big_free(ctx->big_buf, ctx->big_buf_size);
  ctx->big_buf = NULL;
  SORT_XTRACE("Freed sorting buffer");
}

static int sorter_presort(struct sort_context *ctx, struct sort_bucket *in, struct sort_bucket *out, struct sort_bucket *out_only)
{
  /* FIXME: Mode with no presorting (mostly for debugging) */
  sorter_alloc_buf(ctx);
  if (in->flags & SBF_CUSTOM_PRESORT)
    {
      struct fastbuf *f = sbuck_open_write(out);
      return ctx->custom_presort(f, ctx->big_buf, ctx->big_buf_size);	// FIXME: out_only optimization?
    }
  return ctx->internal_sort(ctx, in, out, out_only);
}

static inline struct sort_bucket *
sbuck_join_to(struct sort_bucket *b)
{
  struct sort_bucket *out = (struct sort_bucket *) b->n.prev;	// Such bucket is guaranteed to exist
  return (out->flags & SBF_FINAL) ? out : NULL;
}

static void
sorter_join(struct sort_bucket *b)
{
  struct sort_bucket *join = sbuck_join_to(b);
  ASSERT(join);

  // FIXME: What if the final bucket doesn't contain any file yet?

  SORT_TRACE("Copying %jd bytes to output file", (uintmax_t) b->size);
  struct fastbuf *src = sbuck_open_read(b);
  struct fastbuf *dest = sbuck_open_write(join);
  bbcopy(src, dest, ~0U);
  sbuck_drop(b);
}

static void
sorter_twoway(struct sort_context *ctx, struct sort_bucket *b)
{
  struct sort_bucket *ins[3], *outs[3];
  struct sort_bucket *join = sbuck_join_to(b);

  SORT_TRACE("Presorting");
  ins[0] = sbuck_new(ctx);
  sbuck_open_read(b);
  if (!sorter_presort(ctx, b, ins[0], join ? : ins[0]))
    {
      if (join)
	sbuck_drop(ins[0]);
      else
	clist_insert_after(&ins[0]->n, &b->n);
      sbuck_drop(b);
      return;
    }

  ins[1] = sbuck_new(ctx);
  ins[2] = NULL;
  int i = 1;
  while (sorter_presort(ctx, b, ins[i], ins[i]))
    i = 1-i;
  sbuck_close_read(b);
  sbuck_close_write(ins[0]);
  sbuck_close_write(ins[1]);

  SORT_TRACE("Main sorting");
  do {
    if (ins[0]->runs == 1 && ins[1]->runs == 1 && join)	// FIXME: Debug switch for disabling joining optimizations
      {
	// This is guaranteed to produce a single run, so join if possible
	outs[0] = join;
	outs[1] = NULL;
	ctx->twoway_merge(ctx, ins, outs);
	ASSERT(outs[0]->runs == 2);
	outs[0]->runs--;
	SORT_TRACE("Pass done (joined final run)");
	sbuck_drop(b);
	return;
      }
    outs[0] = sbuck_new(ctx);
    outs[1] = sbuck_new(ctx);
    outs[2] = NULL;
    ctx->twoway_merge(ctx, ins, outs);
    sbuck_close_write(outs[0]);
    sbuck_close_write(outs[1]);
    SORT_TRACE("Pass done (%d+%d runs, %jd+%jd bytes)", outs[0]->runs, outs[1]->runs, (uintmax_t) outs[0]->size, (uintmax_t) outs[1]->size);
    sbuck_drop(ins[0]);
    sbuck_drop(ins[1]);
    memcpy(ins, outs, 3*sizeof(struct sort_bucket *));
  } while (ins[1]->size);

  sbuck_drop(ins[1]);
  clist_insert_after(&ins[0]->n, &b->n);
  sbuck_drop(b);
}

void
sorter_run(struct sort_context *ctx)
{
  ctx->pool = mp_new(4096);
  clist_init(&ctx->bucket_list);

  /* FIXME: There should be a way how to detect size of the input file */
  /* FIXME: Remember to test sorting of empty files */

  // Create bucket containing the source
  struct sort_bucket *bin = sbuck_new(ctx);
  bin->flags = SBF_SOURCE;
  if (ctx->custom_presort)
    bin->flags |= SBF_CUSTOM_PRESORT;
  else
    bin->fb = ctx->in_fb;
  bin->ident = "in";
  bin->size = ~(u64)0;
  bin->hash_bits = ctx->hash_bits;
  clist_add_tail(&ctx->bucket_list, &bin->n);

  // Create bucket for the output
  struct sort_bucket *bout = sbuck_new(ctx);
  bout->flags = SBF_FINAL;
  bout->fb = ctx->out_fb;
  bout->ident = "out";
  bout->runs = 1;
  clist_add_head(&ctx->bucket_list, &bout->n);

  struct sort_bucket *b;
  while (b = clist_next(&ctx->bucket_list, &bout->n))
    {
      if (!b->size)
	sbuck_drop(b);
      else if (b->runs == 1)
	sorter_join(b);
      else
	sorter_twoway(ctx, b);
    }

  sorter_free_buf(ctx);
  sbuck_close_write(bout);
  SORT_XTRACE("Final size: %jd", (uintmax_t) bout->size);
  ctx->out_fb = sbuck_open_read(bout);
}
