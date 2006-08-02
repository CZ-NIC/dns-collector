/*
 *      Image Library -- Duplicates Comparison
 *
 *      (c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *      This software may be freely distributed and used according to the terms
 *      of the GNU Lesser General Public License.
 *
 *
 *      FIXME:
 *      - many possible optimization
 *      - compare normalized pictures (brightness, ...)
 *      - a blur should help to deal with scaling errors
 *      - maybe better/slower last step
 *      - different thresholds for various transformations
 *      - do not test all transformations for symetric pictures
 *      - allocated memory could be easily decreased to about 1/3
 *        for aspect ratio threshold near one
 */

#define LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/mempool.h"
#include "images/images.h"
#include "images/dup-cmp.h"

#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include <fcntl.h>

static uns image_dup_ratio_threshold = 140;
static uns image_dup_error_threshold = 600;
static uns image_dup_tab_limit = 8;

static inline byte *
image_dup_block(struct image_dup *dup, uns tab_col, uns tab_row)
{
  return dup->tab_pixels + (dup->tab_row_size << tab_row) + (3 << (tab_row + tab_col));
}

static inline struct image *
image_dup_subimage(struct image_thread *thread, struct image_dup *dup, struct image *block, uns tab_col, uns tab_row)
{
  return image_init_matrix(thread, block, image_dup_block(dup, tab_col, tab_row),
      1 << tab_col, 1 << tab_row, 3 << tab_col, COLOR_SPACE_RGB);
}

static inline void
pixels_average(byte *dest, byte *src1, byte *src2)
{
  dest[0] = ((uns)src1[0] + (uns)src2[0]) >> 1;
  dest[1] = ((uns)src1[1] + (uns)src2[1]) >> 1;
  dest[2] = ((uns)src1[2] + (uns)src2[2]) >> 1;
}

int
image_dup_init(struct image_thread *thread, struct image_dup *dup, struct image *img, struct mempool *pool)
{
  DBG("image_dup_init()");

  ASSERT((img->flags & IMAGE_PIXEL_FORMAT) == COLOR_SPACE_RGB);

  dup->image = img;
  for (dup->tab_cols = 0; (uns)(2 << dup->tab_cols) < img->cols && dup->tab_cols < image_dup_tab_limit; dup->tab_cols++);
  for (dup->tab_rows = 0; (uns)(2 << dup->tab_rows) < img->rows && dup->tab_rows < image_dup_tab_limit; dup->tab_rows++);
  dup->tab_pixels = mp_alloc(pool, dup->tab_size = (12 << (dup->tab_cols + dup->tab_rows)));
  dup->tab_row_size = 6 << dup->tab_cols;

  /* Scale original image to right bottom block */
  {
    struct image block;
    if (!image_dup_subimage(thread, dup, &block, dup->tab_cols, dup->tab_rows))
      return 0;
    if (!image_scale(thread, &block, img))
      return 0;
  }

  /* Complete bottom row */
  for (uns i = dup->tab_cols; i--; )
    {
      byte *d = image_dup_block(dup, i, dup->tab_rows);
      byte *s = image_dup_block(dup, i + 1, dup->tab_rows);
      for (uns y = 0; y < (uns)(1 << dup->tab_rows); y++)
	for (uns x = 0; x < (uns)(1 << i); x++)
	  {
	    pixels_average(d, s, s + 3);
	    d += 3;
	    s += 6;
	  }
    }

  /* Complete remaining blocks */
  for (uns i = 0; i <= dup->tab_cols; i++)
    {
      uns line_size = (3 << i);
      for (uns j = dup->tab_rows; j--; )
        {
          byte *d = image_dup_block(dup, i, j);
          byte *s = image_dup_block(dup, i, j + 1);
          for (uns y = 0; y < (uns)(1 << j); y++)
            {
              for (uns x = 0; x < (uns)(1 << i); x++)
                {
 	          pixels_average(d, s, s + line_size);
		  d += 3;
		  s += 3;
		}
	      s += line_size;
	    }
        }
    }

  return 1;
}

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
aspect_ratio_test(uns cols1, uns rows1, uns cols2, uns rows2)
{
  DBG("aspect_ratio_test(cols1=%u rows1=%u cols2=%u rows2=%u)", cols1, rows1, cols2, rows2);
  uns r1 = cols1 * rows2;
  uns r2 = rows1 * cols2;
  return
    r1 <= ((r2 * image_dup_ratio_threshold) >> 7) &&
    r2 <= ((r1 * image_dup_ratio_threshold) >> 7);
}

static inline int
average_compare(struct image_dup *dup1, struct image_dup *dup2)
{
  byte *block1 = image_dup_block(dup1, 0, 0);
  byte *block2 = image_dup_block(dup2, 0, 0);
  uns e =
    err(block1[0], block2[0]) +
    err(block1[1], block2[1]) +
    err(block1[2], block2[2]);
  return e <= image_dup_error_threshold;
}

static int
blocks_compare(struct image_dup *dup1, struct image_dup *dup2, uns tab_col, uns tab_row, uns trans)
{
  DBG("blocks_compare(tab_col=%d tab_row=%d trans=%d)", tab_col, tab_row, trans);
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
	return err <= image_dup_error_threshold;
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
  return err <= image_dup_error_threshold;
}

static int
same_size_compare(struct image_dup *dup1, struct image_dup *dup2, uns trans)
{
  struct image *img1 = dup1->image;
  struct image *img2 = dup2->image;
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
  return err <= image_dup_error_threshold;
}

int
image_dup_compare(struct image_dup *dup1, struct image_dup *dup2, uns flags)
{
  DBG("image_dup_compare()");
  if (!average_compare(dup1, dup2))
    return 0;
  struct image *img1 = dup1->image;
  struct image *img2 = dup2->image;
  if (flags & IMAGE_DUP_SCALE)
    {
      DBG("Scale support");
      if (!aspect_ratio_test(img1->cols, img1->rows, img2->cols, img2->rows))
	flags &= ~0x0f;
      if (!aspect_ratio_test(img1->cols, img1->rows, img2->rows, img2->cols))
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
            for (uns i = MAX(cols, rows); i--; )
              {
	        uns col = MAX(0, (int)(cols - i));
	        uns row = MAX(0, (int)(rows - i));
	        if (!blocks_compare(dup1, dup2, col, row, t))
		  break;
		if (!i &&
		    (img1->cols != img2->cols || img1->rows != img2->rows ||
		    same_size_compare(dup1, dup2, t)))
		  {
		    result |= 1 << t;
		    if (!(flags & IMAGE_DUP_WANT_ALL))
		      return result;
		  }
	      }
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
            for (uns i = MAX(cols, rows); i--; )
              {
	        uns col = MAX(0, (int)(cols - i));
	        uns row = MAX(0, (int)(rows - i));
	        if (!blocks_compare(dup1, dup2, col, row, t))
		  break;
		if (!i &&
		    (img1->cols != img2->rows || img1->rows != img2->cols ||
		    same_size_compare(dup1, dup2, t)) )
		  {
		    result |= 1 << t;
		    if (!(flags & IMAGE_DUP_WANT_ALL))
		      return result;
		  }
	      }
	  }
    }
  return result;
}
