/*
 *      Image Library -- Duplicates Comparison
 *
 *      (c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *      This software may be freely distributed and used according to the terms
 *      of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "images/duplicates.h"

#include <fcntl.h>

static inline uns
err (int a, int b)
{
  a -= b;
  return a * a;
}

static inline u64
err_sum(byte *pos1, byte *pos2, uns count)
{
  uns e64 = 0;
  while (count--)
    {
      uns e = err(*pos1++, *pos2++);
      e += err(*pos1++, *pos2++);
      e += err(*pos1++, *pos2++);
      e64 += e;
    }
  return e64;
}

static inline u64
err_sum_transformed(byte *pos1, byte *pos2, uns cols, uns rows, int row_step_1, int col_step_2, int row_step_2)
{
  DBG("err_sum_transformed(pos1=%p pos2=%p cols=%u rows=%u row_step_1=%d col_step_2=%d row_step_2=%d)",
      pos1, pos2, cols, rows, row_step_1, col_step_2, row_step_2);
  u64 e64 = 0;
  for (uns j = rows; j--; )
    {
      byte *p1 = pos1;
      byte *p2 = pos2;
      uns e = 0;
      for (uns i = cols; i--; )
      {
	e += err(p1[0], p2[0]);
	e += err(p1[1], p2[1]);
	e += err(p1[2], p2[2]);
	p1 += 3;
	p2 += col_step_2;
      }
      pos1 += row_step_1;
      pos2 += row_step_2;
      e64 += e;
    }
  return e64;
}

static inline int
aspect_ratio_test(struct image_dup_context *ctx, uns cols1, uns rows1, uns cols2, uns rows2)
{
  DBG("aspect_ratio_test(cols1=%u rows1=%u cols2=%u rows2=%u)", cols1, rows1, cols2, rows2);
  uns r1 = cols1 * rows2;
  uns r2 = rows1 * cols2;
  return
    r1 <= ((r2 * ctx->ratio_threshold) >> 7) &&
    r2 <= ((r1 * ctx->ratio_threshold) >> 7);
}

static inline int
average_compare(struct image_dup_context *ctx, struct image_dup *dup1, struct image_dup *dup2)
{
  byte *block1 = image_dup_block(dup1, 0, 0);
  byte *block2 = image_dup_block(dup2, 0, 0);
  uns e =
    err(block1[0], block2[0]) +
    err(block1[1], block2[1]) +
    err(block1[2], block2[2]);
  return e <= ctx->error_threshold;
}

static int
blocks_compare(struct image_dup_context *ctx, struct image_dup *dup1, struct image_dup *dup2, uns tab_col, uns tab_row, uns trans)
{
  DBG("blocks_compare(tab_col=%d tab_row=%d trans=%d)", tab_col, tab_row, trans);
  ctx->sum_pixels += 1 << (tab_col + tab_row);
  byte *block1 = image_dup_block(dup1, tab_col, tab_row);
  byte *block2;
  int col_step, row_step;
  if (trans < 4)
    block2 = image_dup_block(dup2, tab_col, tab_row);
  else
    block2 = image_dup_block(dup2, tab_row, tab_col);
  switch (trans)
    {
      case 0: ;
	uns err = (err_sum(block1, block2, 1 << (tab_col + tab_row)) >> (tab_col + tab_row));
	DBG("average error=%d", err);
	return err <= ctx->error_threshold;
      case 1:
	col_step = -3;
	row_step = (3 << tab_col);
	block2 += row_step - 3;
	break;
      case 2:
	col_step = 3;
	row_step = -(3 << tab_col);
	block2 += (3 << (tab_col + tab_row)) + row_step;
	break;
      case 3:
	col_step = -3;
	row_step = -(3 << tab_col);
	block2 += (3 << (tab_col + tab_row)) - 3;
	break;
      case 4:
	col_step = (3 << tab_row);
	row_step = 3;
	break;
      case 5:
	col_step = -(3 << tab_row);
	row_step = 3;
	block2 += (3 << (tab_col + tab_row)) + col_step;
	break;
      case 6:
	col_step = (3 << tab_row);
	row_step = -3;
	block2 += col_step - 3;
	break;
      case 7:
	col_step = -(3 << tab_row);
	row_step = -3;
	block2 += (3 << (tab_col + tab_row)) - 3;
	break;
      default:
	ASSERT(0);
    }
  uns err = (err_sum_transformed(block1, block2, (1 << tab_col), (1 << tab_row), (3 << tab_col), col_step, row_step) >> (tab_col + tab_row));
  DBG("average error=%d", err);
  return err <= ctx->error_threshold;
}

static int
same_size_compare(struct image_dup_context *ctx, struct image_dup *dup1, struct image_dup *dup2, uns trans)
{
  struct image *img1 = &dup1->image;
  struct image *img2 = &dup2->image;
  if (!img1->pixels || !img2->pixels)
    return 1;
  ctx->sum_pixels += img1->cols * img1->rows;
  byte *block1 = img1->pixels;
  byte *block2 = img2->pixels;
  int col_step, row_step;
  DBG("same_size_compare(trans=%d)",  trans);
  switch (trans)
    {
      case 0: ;
	col_step = 3;
	row_step = img2->row_size;
	break;
      case 1:
	col_step = -3;
	row_step = img2->row_size;
	block2 += 3 * (img2->cols - 1);
	break;
      case 2:
	col_step = 3;
	row_step = -img2->row_size;
	block2 += img2->row_size * (img2->rows - 1);
	break;
      case 3:
	col_step = -3;
	row_step = -img2->row_size;
	block2 += img2->row_size * (img2->rows - 1) + 3 * (img2->cols - 1);
	break;
      case 4:
	col_step = img2->row_size;
	row_step = 3;
	break;
      case 5:
	col_step = -img2->row_size;
	row_step = 3;
	block2 += img2->row_size * (img2->rows - 1);
	break;
      case 6:
	col_step = img2->row_size;
	row_step = -3;
	block2 += 3 * (img2->cols - 1);
	break;
      case 7:
	col_step = -img2->row_size;
	row_step = -3;
	block2 += img2->row_size * (img2->rows - 1) + 3 * (img2->cols - 1);
	break;
      default:
	ASSERT(0);
    }
  uns err = (err_sum_transformed(block1, block2, img1->cols, img1->rows, img1->row_size, col_step, row_step) / ((u64)img1->cols * img1->rows));
  DBG("average error=%d", err);
  return err <= ctx->error_threshold;
}

uns
image_dup_compare(struct image_dup_context *ctx, struct image_dup *dup1, struct image_dup *dup2)
{
  DBG("image_dup_compare()");
  if (!average_compare(ctx, dup1, dup2))
    return 0;
  struct image *img1 = &dup1->image;
  struct image *img2 = &dup2->image;
  uns flags = ctx->flags;
  if (flags & IMAGE_DUP_SCALE)
    {
      DBG("Scale support");
      if (!aspect_ratio_test(ctx, img1->cols, img1->rows, img2->cols, img2->rows))
	flags &= ~0x0f;
      if (!aspect_ratio_test(ctx, img1->cols, img1->rows, img2->rows, img2->cols))
	flags &= ~0xf0;
    }
  else
    {
      DBG("No scale support");
      if (!(img1->cols == img2->cols && img1->rows == img2->rows))
	flags &= ~0x0f;
      if (!(img1->cols == img2->rows && img1->rows == img2->cols))
	flags &= ~0xf0;
    }
  if (!(flags & 0xff))
    return 0;
  uns result = 0;
  if (flags & 0x0f)
    {
      uns cols = MIN(dup1->tab_cols, dup2->tab_cols);
      uns rows = MIN(dup1->tab_rows, dup2->tab_rows);
      for (uns t = 0; t < 4; t++)
	if (flags & (1 << t))
	  {
	    DBG("Testing trans %d", t);
	    uns i = MAX(cols, rows), depth = 1;
            while (i--)
              {
		depth++;
	        uns col = MAX(0, (int)(cols - i));
	        uns row = MAX(0, (int)(rows - i));
	        if (!blocks_compare(ctx, dup1, dup2, col, row, t))
		  break;
		if (!i &&
		    (img1->cols != img2->cols || img1->rows != img2->rows ||
		    same_size_compare(ctx, dup1, dup2, t)))
		  {
		    result |= 1 << t;
		    if (!(flags & IMAGE_DUP_WANT_ALL))
		      return result;
		    else
		      break;
		  }
	      }
	    ctx->sum_depth += depth;
	  }
    }
  if (flags & 0xf0)
    {
      uns cols = MIN(dup1->tab_cols, dup2->tab_rows);
      uns rows = MIN(dup1->tab_rows, dup2->tab_cols);
      for (uns t = 4; t < 8; t++)
	if (flags & (1 << t))
	  {
	    DBG("Testing trans %d", t);
	    uns i = MAX(cols, rows), depth = 1;
            while (i--)
              {
		depth++;
	        uns col = MAX(0, (int)(cols - i));
	        uns row = MAX(0, (int)(rows - i));
	        if (!blocks_compare(ctx, dup1, dup2, col, row, t))
		  break;
		if (!i &&
		    (img1->cols != img2->rows || img1->rows != img2->cols ||
		    same_size_compare(ctx, dup1, dup2, t)) )
		  {
		    result |= 1 << t;
		    if (!(flags & IMAGE_DUP_WANT_ALL))
		      return result;
		    else
		      break;
		  }
	      }
	    ctx->sum_depth += depth;
	  }
    }
  return result;
}
