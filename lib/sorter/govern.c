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

#include <fcntl.h>

void *
sorter_alloc(struct sort_context *ctx, uns size)
{
  return mp_alloc_zero(ctx->pool, size);
}

struct sort_bucket *
sbuck_new(struct sort_context *ctx)
{
  struct sort_bucket *b = sorter_alloc(ctx, sizeof(struct sort_bucket));
  b->ctx = ctx;
  return b;
}

void
sbuck_drop(struct sort_bucket *b)
{
  if (b)
    {
      ASSERT(!(b->flags & SBF_DESTROYED));
      if (b->n.prev)
	clist_remove(&b->n);
      bclose(b->fb);
      bzero(b, sizeof(*b));
      b->flags = SBF_DESTROYED;
    }
}

sh_off_t
sbuck_size(struct sort_bucket *b)
{
  if ((b->flags & SBF_OPEN_WRITE) && !(b->flags & SBF_SWAPPED_OUT))
    return btell(b->fb);
  else
    return b->size;
}

int
sbuck_have(struct sort_bucket *b)
{
  return b && sbuck_size(b);
}

static void
sbuck_swap_in(struct sort_bucket *b)
{
  if (b->flags & SBF_SWAPPED_OUT)
    {
      b->fb = bopen(b->filename, O_RDWR, sorter_stream_bufsize);
      if (b->flags & SBF_OPEN_WRITE)
	bseek(b->fb, 0, SEEK_END);
      bconfig(b->fb, BCONFIG_IS_TEMP_FILE, 1);
      b->flags &= ~SBF_SWAPPED_OUT;
      SORT_XTRACE("Swapped in %s", b->filename);
    }
}

struct fastbuf *
sbuck_read(struct sort_bucket *b)
{
  sbuck_swap_in(b);
  if (b->flags & SBF_OPEN_READ)
    return b->fb;
  else if (b->flags & SBF_OPEN_WRITE)
    {
      b->size = btell(b->fb);
      b->flags = (b->flags & ~SBF_OPEN_WRITE) | SBF_OPEN_READ;
      brewind(b->fb);
      return b->fb;
    }
  else
    ASSERT(0);
}

struct fastbuf *
sbuck_write(struct sort_bucket *b)
{
  sbuck_swap_in(b);
  if (b->flags & SBF_OPEN_WRITE)
    ASSERT(b->fb);
  else
    {
      ASSERT(!(b->flags & (SBF_OPEN_READ | SBF_DESTROYED)));
      b->fb = bopen_tmp(sorter_stream_bufsize);
      b->flags |= SBF_OPEN_WRITE;
      b->filename = mp_strdup(b->ctx->pool, b->fb->name);
    }
  return b->fb;
}

void
sbuck_swap_out(struct sort_bucket *b)
{
  if ((b->flags & (SBF_OPEN_READ | SBF_OPEN_WRITE)) && b->fb)
    {
      if (b->flags & SBF_OPEN_WRITE)
	b->size = btell(b->fb);
      bconfig(b->fb, BCONFIG_IS_TEMP_FILE, 0);
      bclose(b->fb);
      b->fb = NULL;
      b->flags |= SBF_SWAPPED_OUT;
      SORT_XTRACE("Swapped out %s", b->filename);
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
      struct fastbuf *f = sbuck_write(out);
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

  SORT_TRACE("Copying %jd bytes to output file", (uintmax_t) sbuck_size(b));
  struct fastbuf *src = sbuck_read(b);
  struct fastbuf *dest = sbuck_write(join);
  bbcopy(src, dest, ~0U);
  sbuck_drop(b);
}

static void
sorter_twoway(struct sort_context *ctx, struct sort_bucket *b)
{
  struct sort_bucket *ins[3] = { NULL }, *outs[3] = { NULL };
  cnode *list_pos = b->n.prev;
  struct sort_bucket *join = sbuck_join_to(b);
  if (sorter_debug & SORT_DEBUG_NO_JOIN)
    join = NULL;

  if (!(sorter_debug & SORT_DEBUG_NO_PRESORT) || (b->flags & SBF_CUSTOM_PRESORT))
    {
      SORT_TRACE("Presorting");
      ins[0] = sbuck_new(ctx);
      if (!sorter_presort(ctx, b, ins[0], join ? : ins[0]))
	{
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
    }
  else
    {
      SORT_TRACE("Skipped presorting");
      ins[0] = b;
    }

  SORT_TRACE("Main sorting");
  do {
    if (ins[0]->runs == 1 && ins[1]->runs == 1 && join)
      {
	// This is guaranteed to produce a single run, so join if possible
	outs[0] = join;
	outs[1] = NULL;
	ctx->twoway_merge(ctx, ins, outs);
	ASSERT(outs[0]->runs == 2);
	outs[0]->runs--;
	SORT_TRACE("Pass done (joined final run)");
	sbuck_drop(ins[0]);
	sbuck_drop(ins[1]);
	return;
      }
    outs[0] = sbuck_new(ctx);
    outs[1] = sbuck_new(ctx);
    outs[2] = NULL;
    ctx->twoway_merge(ctx, ins, outs);
    SORT_TRACE("Pass done (%d+%d runs, %jd+%jd bytes)", outs[0]->runs, outs[1]->runs, (uintmax_t) sbuck_size(outs[0]), (uintmax_t) sbuck_size(outs[1]));
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

  /* FIXME: There should be a way how to detect size of the input file */
  /* FIXME: Remember to test sorting of empty files */

  // Create bucket containing the source
  struct sort_bucket *bin = sbuck_new(ctx);
  bin->flags = SBF_SOURCE | SBF_OPEN_READ;
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
      if (!sbuck_have(b))
	sbuck_drop(b);
      else if (b->runs == 1)
	sorter_join(b);
      else
	sorter_twoway(ctx, b);
    }

  sorter_free_buf(ctx);
  SORT_XTRACE("Final size: %jd", (uintmax_t) sbuck_size(bout));
  ctx->out_fb = sbuck_read(bout);
}
