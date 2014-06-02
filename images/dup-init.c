/*
 *      Image Library -- Duplicates Comparison
 *
 *      (c) 2006--2007 Pavel Charvat <pchar@ucw.cz>
 *
 *      This software may be freely distributed and used according to the terms
 *      of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/fastbuf.h>
#include <images/images.h>
#include <images/color.h>
#include <images/duplicates.h>

#include <fcntl.h>

void
image_dup_context_init(struct image_context *ic, struct image_dup_context *ctx)
{
  *ctx = (struct image_dup_context) {
    .ic = ic,
    .flags = IMAGE_DUP_TRANS_ID,
    .ratio_threshold = 140,
    .error_threshold = 100,
    .qtree_limit = 8,
  };
}

void
image_dup_context_cleanup(struct image_dup_context *ctx UNUSED)
{
}

static inline struct image *
image_dup_subimage(struct image_context *ctx, struct image_dup *dup, struct image *block, uint tab_col, uint tab_row)
{
  return image_init_matrix(ctx, block, image_dup_block(dup, tab_col, tab_row),
      1 << tab_col, 1 << tab_row, 3 << tab_col, COLOR_SPACE_RGB);
}

static inline void
pixels_average(byte *dest, byte *src1, byte *src2)
{
  dest[0] = ((uint)src1[0] + (uint)src2[0]) >> 1;
  dest[1] = ((uint)src1[1] + (uint)src2[1]) >> 1;
  dest[2] = ((uint)src1[2] + (uint)src2[2]) >> 1;
}

uint
image_dup_estimate_size(uint cols, uint rows, uint same_size_compare, uint qtree_limit)
{
  uint tab_cols, tab_rows;
  for (tab_cols = 0; (uint)(2 << tab_cols) < cols && tab_cols < qtree_limit; tab_cols++);
  for (tab_rows = 0; (uint)(2 << tab_rows) < rows && tab_rows < qtree_limit; tab_rows++);
  uint size = sizeof(struct image_dup) + (12 << (tab_cols + tab_rows)) + 2 * CPU_STRUCT_ALIGN;
  if (same_size_compare)
    size += cols * rows * 3 + CPU_STRUCT_ALIGN;
  return ALIGN_TO(size, CPU_STRUCT_ALIGN);
}

uint
image_dup_new(struct image_dup_context *ctx, struct image *img, void *buffer, uint same_size_compare)
{
  DBG("image_dup_init()");
  ASSERT(!((uintptr_t)buffer & (CPU_STRUCT_ALIGN - 1)));
  void *ptr = buffer;

  /* Allocate the structure */
  struct image_dup *dup = ptr;
  ptr += ALIGN_TO(sizeof(*dup), CPU_STRUCT_ALIGN);
  bzero(dup, sizeof(*dup));

  ASSERT((img->flags & IMAGE_PIXEL_FORMAT) == COLOR_SPACE_RGB);

  /* Clone image */
  if (same_size_compare)
    {
      if (!image_init_matrix(ctx->ic, &dup->image, ptr, img->cols, img->rows, img->cols * 3, COLOR_SPACE_RGB))
        return 0;
      uint size = img->rows * img->cols * 3;
      ptr += ALIGN_TO(size, CPU_STRUCT_ALIGN);
      byte *s = img->pixels;
      byte *d = dup->image.pixels;
      for (uint row = img->rows; row--; )
        {
	  memcpy(d, s, img->row_pixels_size);
	  d += dup->image.row_size;
	  s += img->row_size;
	}
    }
  else
    {
      dup->image.cols = img->cols;
      dup->image.rows = img->rows;
    }

  for (dup->tab_cols = 0; (uint)(2 << dup->tab_cols) < img->cols && dup->tab_cols < ctx->qtree_limit; dup->tab_cols++);
  for (dup->tab_rows = 0; (uint)(2 << dup->tab_rows) < img->rows && dup->tab_rows < ctx->qtree_limit; dup->tab_rows++);
  dup->tab_row_size = 6 << dup->tab_cols;
  dup->tab_pixels = ptr;
  uint size = 12 << (dup->tab_cols + dup->tab_rows);
  ptr += ALIGN_TO(size, CPU_STRUCT_ALIGN);

  /* Scale original image to right bottom block */
  {
    struct image block;
    if (!image_dup_subimage(ctx->ic, dup, &block, dup->tab_cols, dup->tab_rows))
      return 0;
    if (!image_scale(ctx->ic, &block, img))
      return 0;
  }

  /* Complete bottom row */
  for (uint i = dup->tab_cols; i--; )
    {
      byte *d = image_dup_block(dup, i, dup->tab_rows);
      byte *s = image_dup_block(dup, i + 1, dup->tab_rows);
      for (uint y = 0; y < (uint)(1 << dup->tab_rows); y++)
	for (uint x = 0; x < (uint)(1 << i); x++)
	  {
	    pixels_average(d, s, s + 3);
	    d += 3;
	    s += 6;
	  }
    }

  /* Complete remaining blocks */
  for (uint i = 0; i <= dup->tab_cols; i++)
    {
      uint line_size = (3 << i);
      for (uint j = dup->tab_rows; j--; )
        {
          byte *d = image_dup_block(dup, i, j);
          byte *s = image_dup_block(dup, i, j + 1);
          for (uint y = 0; y < (uint)(1 << j); y++)
            {
              for (uint x = 0; x < (uint)(1 << i); x++)
                {
	          pixels_average(d, s, s + line_size);
		  d += 3;
		  s += 3;
		}
	      s += line_size;
	    }
        }
    }

  return ptr - buffer;
}
