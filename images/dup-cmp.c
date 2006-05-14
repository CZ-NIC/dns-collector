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
 *      - better image scale... now it can completely miss some rows/cols of pixels
 *      - maybe better/slower last step
 *      - different thresholds for various transformations
 *      - do not test all transformations for symetric pictures
 *      - allocated memory could be easily decreased to about 1/3 
 *        for aspect ratio threshold near one
 *      - ... secret ideas :-)
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/mempool.h"
#include "images/images.h"
#include "images/dup-cmp.h"

static uns image_dup_scale_min_size = 16;
static uns image_dup_ratio_threshold = 140;
static uns image_dup_error_threshold = 50;

static inline byte *
image_dup_block(struct image_dup *dup, uns col, uns row)
{
  ASSERT(col <= dup->cols && row <= dup->rows);
  return dup->buf + (dup->line << row) + (3 << (row + col));
}

static inline void
pixels_average(byte *dest, byte *src1, byte *src2)
{
  dest[0] = ((uns)src1[0] + (uns)src2[0]) >> 1;
  dest[1] = ((uns)src1[1] + (uns)src2[1]) >> 1;
  dest[2] = ((uns)src1[2] + (uns)src2[2]) >> 1;
}

uns
image_dup_estimate_size(uns width, uns height)
{
  uns cols, rows;
  for (cols = 0; (uns)(2 << cols) < width; cols++);
  for (rows = 0; (uns)(2 << rows) < height; rows++);
  return sizeof(struct image_dup) + (12 << (cols + rows));
}

void
image_dup_init(struct image_dup *dup, struct image_data *image, struct mempool *pool)
{
  ASSERT(image->width && image->height);
  
  dup->image = image;
  dup->width = image->width;
  dup->height = image->height;
  for (dup->cols = 0; (uns)(2 << dup->cols) < image->width; dup->cols++);
  for (dup->rows = 0; (uns)(2 << dup->rows) < image->height; dup->rows++);
  dup->buf = mp_alloc(pool, dup->buf_size = (12 << (dup->cols + dup->rows)));
  dup->line = 6 << dup->cols;
  dup->flags = 0;
  if (image->width >= image_dup_scale_min_size && image->height >= image_dup_scale_min_size)
    dup->flags |= IMAGE_DUP_FLAG_SCALE;
  
  /* Scale original image to right bottom block */
  {
    byte *d = image_dup_block(dup, dup->cols, dup->rows);
    uns width = 1 << dup->cols;
    uns height = 1 << dup->rows;
    uns line_size = 3 * image->width;
    uns src_y = 0;
    for (uns y = 0; y < height; y++)
      {
	byte *line = image->pixels + line_size * (src_y >> dup->rows);
        uns src_x = 0;
        for (uns x = 0; x < width; x++)
          {
	    byte *s = line + 3 * (src_x >> dup->cols);
	    d[0] = s[0];
	    d[1] = s[1];
	    d[2] = s[2];
	    d += 3;
	    src_x += image->width;
	  }
	src_y += image->height;
      }
  }

  /* Complete bottom row */
  for (uns i = dup->cols; i--; )
    {
      byte *d = image_dup_block(dup, i, dup->rows);
      byte *s = image_dup_block(dup, i + 1, dup->rows);
      for (uns y = 0; y < (uns)(1 << dup->rows); y++)
	for (uns x = 0; x < (uns)(1 << i); x++)
	  {
	    pixels_average(d, s, s + 3);
	    d += 3;
	    s += 6;
	  }
    }
 
  /* Complete remaining blocks */
  for (uns i = 0; i <= dup->cols; i++)
    {
      uns line_size = (3 << i);
      for (uns j = dup->rows; j--; )
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
}

static inline uns
err (int a, int b)
{
  a -= b;
  return a * a;
}

static inline uns
err_sum(byte *pos1, byte *end1, byte *pos2)
{
  uns e = 0;
  while (pos1 != end1)
    e += err(*pos1++, *pos2++);
  return e;
}

static inline uns
err_sum_transformed(byte *pos1, byte *end1, byte *pos2, uns width, int add1, int add2)
{
  DBG("err_sum_transformed(): %p %p %p %d %d %d", pos1, end1, pos2, width, add1, add2);
  uns e = 0;
  while (pos1 != end1)
    {
      for (uns i = 0; i < width; i++, pos2 += add1)
      {
	e += err(pos1[0], pos2[0]);
	e += err(pos1[1], pos2[1]);
	e += err(pos1[2], pos2[2]);
	pos1 += 3;
      }
      pos2 += add2;
    }
  return e;
}

static inline int
aspect_ratio_test(uns width1, uns height1, uns width2, uns height2)
{
  uns r1 = width1 * height2;
  uns r2 = height1 * width2;
  return
    r1 <= ((r2 * image_dup_ratio_threshold) >> 5) && 
    r2 <= ((r1 * image_dup_ratio_threshold) >> 5);
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
blocks_compare(struct image_dup *dup1, struct image_dup *dup2, uns col, uns row, uns trans)
{
  DBG("blocks_compare(): col=%d row=%d trans=%d", col, row, trans);
  byte *block1 = image_dup_block(dup1, col, row);
  byte *block2 = (trans < 4) ? image_dup_block(dup2, col, row) : image_dup_block(dup2, row, col);
  int add1, add2;
  switch (trans)
    {
      case 0: ;
	uns err = (err_sum(block1, block1 + (3 << (col + row)), block2) >> (col + row));
	DBG("average error=%d", err);
	return err <= image_dup_error_threshold;
      case 1:
	add1 = -3;
	add2 = 6 << col;
	block2 += (3 << col) - 3;
	break;
      case 2:
	add1 = 1;
	add2 = -(6 << col);
	block2 += (3 << (col + row)) - (3 << col);
	break;
      case 3:
	add1 = -3;
	add2 = 0;
	block2 += (3 << (col + row)) - 3;
	break;
      case 4:
	add1 = (3 << col);
	add2 = -(3 << (col + row)) + 3;
	break;
      case 5:
	add1 = -(3 << col);
	add2 = (3 << (col + row)) + 3;
	block2 += (3 << (col + row)) - (3 << col);
	break;
      case 6:
	add1 = (3 << col);
	add2 = -(3 << (col + row)) - 3;
	block2 += (3 << col) - 3;
	break;
      case 7:
	add1 = -(3 << col);
	add2 = (3 << (col + row)) - 3;
	block2 += (3 << (col + row)) - 3;
	break;
      default:
	ASSERT(0);
    }
  uns err = (err_sum_transformed(block1, block1 + (3 << (col + row)), block2, (1 << col), add1, add2) >> (col + row));
  DBG("average error=%d", err);
  return err <= image_dup_error_threshold;
}

static int
same_size_compare(struct image_dup *dup1, struct image_dup *dup2, uns trans)
{
  byte *block1 = dup1->image->pixels;
  byte *block2 = dup2->image->pixels;
  DBG("same_size_compare(): trans=%d",  trans);
  int add1, add2;
  switch (trans)
    {
      case 0: ;
        uns err = (err_sum(block1, block1 + 3 * dup1->width * dup1->height, block2) / (dup1->width * dup1->height));
	DBG("average error=%d", err);
	return err <= image_dup_error_threshold;
      case 1:
	add1 = -3;
	add2 = 6 * dup1->width;
	block2 += 3 * (dup1->width - 1);
	break;
      case 2:
	add1 = 1;
	add2 = -6 * dup1->width;
	block2 += 3 * dup1->width * (dup1->height - 1);
	break;
      case 3:
	add1 = -3;
	add2 = 0;
	block2 += 3 * (dup1->width * dup1->height - 1);
	break;
      case 4:
	add1 = 3 * dup1->width;
	add2 = -3 * (dup1->width * dup1->height - 1);
	break;
      case 5:
	add1 = -3 * dup1->width;
	add2 = 3 * (dup1->width * dup1->height + 1);
	block2 += 3 * dup1->width * (dup1->height - 1);
	break;
      case 6:
	add1 = 3 * dup1->width;
	add2 = -3 * (dup1->width * dup1->height + 1);
	block2 += 3 * (dup1->width - 1);
	break;
      case 7:
	add1 = -3 * dup1->width;
	add2 = 3 * (dup1->width * dup1->height - 1);
	block2 += 3 * (dup1->width * dup1->height - 1);
	break;
      default:
	ASSERT(0);
    }
  uns err = (err_sum_transformed(block1, block1 + 3 * dup1->width * dup1->height, block2, dup1->width, add1, add2) / (dup1->width * dup1->height));
  DBG("average error=%d", err);
  return err <= image_dup_error_threshold;
}

int
image_dup_compare(struct image_dup *dup1, struct image_dup *dup2, uns trans)
{
  if (!average_compare(dup1, dup2))
    return 0;
  if ((dup1->flags & dup2->flags) & IMAGE_DUP_FLAG_SCALE)
    {
      DBG("Scale support");
      if (!aspect_ratio_test(dup1->width, dup1->height, dup2->width, dup2->height))
	trans &= 0xf0;
      if (!aspect_ratio_test(dup1->width, dup1->height, dup2->height, dup2->width))
	trans &= 0x0f;
    }
  else
    {
      DBG("No scale support");
      if (!(dup1->width == dup2->width && dup1->height == dup2->height))
	trans &= 0xf0;
      if (!(dup1->width == dup2->height && dup1->height == dup2->width))
	trans &= 0x0f;
    }
  if (!trans)
    return 0;
  if (trans & 0x0f)
    {
      uns cols = MIN(dup1->cols, dup2->cols);
      uns rows = MIN(dup1->rows, dup2->rows);
      for (uns t = 0; t < 4; t++)
	if (trans & (1 << t))
	  {
	    DBG("Testing trans %d", t);
            for (uns i = MAX(cols, rows); i--; )
              {
	        uns col = MAX(0, (int)(cols - i));
	        uns row = MAX(0, (int)(rows - i));
	        if (!blocks_compare(dup1, dup2, col, row, t))
		  break;
		if (!i &&
		    (dup1->width != dup2->width || dup1->height != dup2->height ||
		    same_size_compare(dup1, dup2, t)))
		  return 1;
	      }
	  }
    }
  if (trans & 0xf0)
    {
      uns cols = MIN(dup1->cols, dup2->rows);
      uns rows = MIN(dup1->rows, dup2->cols);
      for (uns t = 4; t < 8; t++)
	if (trans & (1 << t))
	  {
	    DBG("Testing trans %d", t);
            for (uns i = MAX(cols, rows); i--; )
              {
	        uns col = MAX(0, (int)(cols - i));
	        uns row = MAX(0, (int)(rows - i));
	        if (!blocks_compare(dup1, dup2, col, row, t))
		  break;
		if (!i &&
		    (dup1->width != dup2->height || dup1->height != dup2->width ||
		    same_size_compare(dup1, dup2, t)) )
		  return 1;
	      }
	  }
    }
  return 0;
}
