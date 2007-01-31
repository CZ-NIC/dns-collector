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

struct sort_bucket *
sorter_new_bucket(struct sort_context *ctx)
{
  return mp_alloc_zero(ctx->pool, sizeof(struct sort_bucket));
}

struct fastbuf *
sorter_open_read(struct sort_bucket *b)
{
  /* FIXME: These functions should handle buckets with no fb and only name. */
  ASSERT(b->fb);
  return b->fb;
}

struct fastbuf *
sorter_open_write(struct sort_bucket *b)
{
  if (!b->fb)
    b->fb = bopen_tmp(sorter_stream_bufsize);
  return b->fb;
}

void
sorter_close_read(struct sort_bucket *b)
{
  if (!b)
    return;
  ASSERT(b->fb);
  bclose(b->fb);
  b->fb = NULL;
}

void
sorter_close_write(struct sort_bucket *b)
{
  if (b->fb)
    {
      b->size = btell(b->fb);
      brewind(b->fb);
    }
  /* FIXME: Remove empty buckets from the list automatically? */
}

void
sorter_run(struct sort_context *ctx)
{
  ctx->pool = mp_new(4096);
  ASSERT(!ctx->custom_presort);
  ASSERT(!ctx->out_fb);
  clist_init(&ctx->bucket_list);

  /* FIXME: There should be a way how to detect size of the input file */

  /* Trivial 2-way merge with no presorting (just a testing hack) */
  struct sort_bucket *bin = sorter_new_bucket(ctx);
  bin->flags = SBF_SOURCE;
  bin->fb = ctx->in_fb;
  bin->ident = "src";
  struct sort_bucket *ins[3], *outs[3];
  ins[0] = bin;
  ins[1] = NULL;

  do {
    outs[0] = sorter_new_bucket(ctx);
    outs[1] = sorter_new_bucket(ctx);
    outs[2] = NULL;
    log(L_DEBUG, "Pass...");
    ctx->twoway_merge(ctx, ins, outs);
    log(L_DEBUG, "Done (%d+%d runs)", outs[0]->runs, outs[1]->runs);
    sorter_close_write(outs[0]);
    sorter_close_write(outs[1]);
    memcpy(ins, outs, 3*sizeof(struct sort_bucket *));
  } while (ins[1]->fb);

  ctx->out_fb = sorter_open_read(ins[0]);
}
