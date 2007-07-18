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
#include "images/color.h"
#include "images/duplicates.h"

#include <fcntl.h>

static uns image_dup_tab_limit = 8;

static inline struct image *
image_dup_subimage(struct image_context *ctx, struct image_dup *dup, struct image *block, uns tab_col, uns tab_row)
{
  return image_init_matrix(ctx, block, image_dup_block(dup, tab_col, tab_row),
      1 << tab_col, 1 << tab_row, 3 << tab_col, COLOR_SPACE_RGB);
}

static inline void
pixels_average(byte *dest, byte *src1, byte *src2)
{
  dest[0] = ((uns)src1[0] + (uns)src2[0]) >> 1;
  dest[1] = ((uns)src1[1] + (uns)src2[1]) >> 1;
  dest[2] = ((uns)src1[2] + (uns)src2[2]) >> 1;
}

uns
image_dup_estimate_size(uns cols, uns rows)
{
  uns tab_cols, tab_rows;
  for (tab_cols = 0; (uns)(2 << tab_cols) < cols && tab_cols < image_dup_tab_limit; tab_cols++);
  for (tab_rows = 0; (uns)(2 << tab_rows) < rows && tab_rows < image_dup_tab_limit; tab_rows++);
  return sizeof(struct image) + cols * rows * 3 + sizeof(struct image_dup) + (12 << (tab_cols + tab_rows)) + 64;
}

uns
image_dup_init(struct image_context *ctx, struct image_dup *dup, struct image *img, struct mempool *pool)
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
    if (!image_dup_subimage(ctx, dup, &block, dup->tab_cols, dup->tab_rows))
      return 0;
    if (!image_scale(ctx, &block, img))
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
