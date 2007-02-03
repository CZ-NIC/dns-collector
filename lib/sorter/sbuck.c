/*
 *	UCW Library -- Universal Sorter: Operations on Contexts, Buffers and Buckets
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

int
sbuck_has_file(struct sort_bucket *b)
{
  return (b->fb || (b->flags & SBF_SWAPPED_OUT));
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
