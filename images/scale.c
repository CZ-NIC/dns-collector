/*
 *	Image Library -- Image scaling algorithms
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "images/images.h"
#include "images/error.h"
#include "images/math.h"

#include <string.h>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#define LINEAR_INTERPOLATE(a, b, t) (((int)((a) << 16) + (int)(t) * ((int)(b) - (int)(a)) + 0x8000) >> 16)

/* Generate optimized code for various pixel formats */

#define IMAGE_SCALE_PREFIX(x) image_scale_1_##x
#define IMAGE_SCALE_PIXEL_SIZE 1
#include "images/scale-gen.h"

#define IMAGE_SCALE_PREFIX(x) image_scale_2_##x
#define IMAGE_SCALE_PIXEL_SIZE 2
#include "images/scale-gen.h"

#define IMAGE_SCALE_PREFIX(x) image_scale_3_##x
#define IMAGE_SCALE_PIXEL_SIZE 3
#include "images/scale-gen.h"

#define IMAGE_SCALE_PREFIX(x) image_scale_4_##x
#define IMAGE_SCALE_PIXEL_SIZE 4
#include "images/scale-gen.h"

/* Simple "nearest neighbour" algorithm */

static void
image_scale_nearest_xy(struct image *dest, struct image *src)
{
  switch (src->pixel_size)
    {
      case 1:
	image_scale_1_nearest_xy(dest, src);
	return;
      case 2:
	image_scale_2_nearest_xy(dest, src);
	return;
      case 3:
	image_scale_3_nearest_xy(dest, src);
	return;
      case 4:
	image_scale_4_nearest_xy(dest, src);
	return;
      default:
	ASSERT(0);
    }
}

static inline void
image_scale_nearest_x(struct image *dest, struct image *src)
{
  image_scale_nearest_xy(dest, src);
}

static void
image_scale_nearest_y(struct image *dest, struct image *src)
{
  uns y_inc = (src->rows << 16) / dest->rows;
  uns y_pos = y_inc >> 1;
  byte *dest_pos = dest->pixels;
  for (uns row_counter = dest->rows; row_counter--; )
    {
      byte *src_pos = src->pixels + (y_pos >> 16) * src->row_size;
      y_pos += y_inc;
      memcpy(dest_pos, src_pos, dest->row_pixels_size);
      dest_pos += dest->row_size;
    }
}

/* Bilinear filter */

UNUSED static void
image_scale_linear_y(struct image *dest, struct image *src)
{
  byte *dest_row = dest->pixels;
  /* Handle problematic special case */
  if (src->rows == 1)
    {
      for (uns y_counter = dest->rows; y_counter--; dest_row += dest->row_size)
        memcpy(dest_row, src->pixels, src->row_pixels_size);
      return;
    }
  /* Initialize the main loop */
  uns y_inc  = ((src->rows - 1) << 16) / (dest->rows - 1), y_pos = 0;
#ifdef __SSE2__
  __m128i zero = _mm_setzero_si128();
#endif
  /* Main loop */
  for (uns y_counter = dest->rows; --y_counter; )
    {
      uns coef = y_pos & 0xffff;
      byte *src_row_1 = src->pixels + (y_pos >> 16) * src->row_size;
      byte *src_row_2 = src_row_1 + src->row_size;
      uns i = 0;
#ifdef __SSE2__
      /* SSE2 */
      __m128i sse_coef = _mm_set1_epi16(coef >> 9);
      for (; (int)i < (int)dest->row_pixels_size - 15; i += 16)
        {
	  __m128i a2 = _mm_loadu_si128((__m128i *)(src_row_1 + i));
	  __m128i a1 = _mm_unpacklo_epi8(a2, zero);
	  a2 = _mm_unpackhi_epi8(a2, zero);
	  __m128i b2 = _mm_loadu_si128((__m128i *)(src_row_2 + i));
	  __m128i b1 = _mm_unpacklo_epi8(b2, zero);
	  b2 = _mm_unpackhi_epi8(b2, zero);
	  b1 = _mm_sub_epi16(b1, a1);
	  b2 = _mm_sub_epi16(b2, a2);
	  a1 = _mm_slli_epi16(a1, 7);
	  a2 = _mm_slli_epi16(a2, 7);
	  b1 = _mm_mullo_epi16(b1, sse_coef);
	  b2 = _mm_mullo_epi16(b2, sse_coef);
	  a1 = _mm_add_epi16(a1, b1);
	  a2 = _mm_add_epi16(a2, b2);
	  a1 = _mm_srli_epi16(a1, 7);
	  a2 = _mm_srli_epi16(a2, 7);
	  a1 = _mm_packus_epi16(a1, a2);
	  _mm_storeu_si128((__m128i *)(dest_row + i), a1);
	}
#endif
      /* Unrolled loop using general-purpose registers */
      for (; (int)i < (int)dest->row_pixels_size - 3; i += 4)
        {
	  dest_row[i + 0] = LINEAR_INTERPOLATE(src_row_1[i + 0], src_row_2[i + 0], coef);
	  dest_row[i + 1] = LINEAR_INTERPOLATE(src_row_1[i + 1], src_row_2[i + 1], coef);
	  dest_row[i + 2] = LINEAR_INTERPOLATE(src_row_1[i + 2], src_row_2[i + 2], coef);
	  dest_row[i + 3] = LINEAR_INTERPOLATE(src_row_1[i + 3], src_row_2[i + 3], coef);
	}
      /* Remaining columns */
      for (; i < dest->row_pixels_size; i++)
	dest_row[i] = LINEAR_INTERPOLATE(src_row_1[i], src_row_2[i], coef);
      dest_row += dest->row_size;
      y_pos += y_inc;
    }
  /* Always copy the last row - faster and also handle "y_pos == dest->rows * 0x10000" overflow */
  memcpy(dest_row, src->pixels + src->image_size - src->row_size, src->row_pixels_size);
}

/* Box filter */

static void
image_scale_downsample_xy(struct image *dest, struct image *src)
{
  switch (src->pixel_size)
    {
      case 1:
	image_scale_1_downsample_xy(dest, src);
	return;
      case 2:
	image_scale_2_downsample_xy(dest, src);
	return;
      case 3:
	image_scale_3_downsample_xy(dest, src);
	return;
      case 4:
	image_scale_4_downsample_xy(dest, src);
	return;
      default:
	ASSERT(0);
    }
}

/* General routine
 * FIXME: customizable; implement at least bilinear and bicubic filters */

int
image_scale(struct image_context *ctx, struct image *dest, struct image *src)
{
  if ((src->flags & IMAGE_PIXEL_FORMAT) != (dest->flags & IMAGE_PIXEL_FORMAT))
    {
      IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Different pixel formats not supported.");
      return 0;
    }
  if (dest->cols == src->cols)
    {
      if (dest->rows == src->rows)
        {
	  /* No scale, copy only */
	  image_scale_nearest_y(dest, src);
	  return 1;
	}
      else if (dest->rows < src->rows)
        {
	  /* Downscale vertically */
	  image_scale_downsample_xy(dest, src);
	  return 1;
	}
      else
        {
	  /* Upscale vertically */
	  image_scale_nearest_y(dest, src);
	  return 1;
	}
    }
  else if (dest->rows == src->rows)
    {
      if (dest->cols < src->cols)
        {
          /* Downscale horizontally */
          image_scale_downsample_xy(dest, src);
          return 1;
	}
      else
        {
	  /* Upscale horizontally */
	  image_scale_nearest_x(dest, src);
	  return 1;
	}
    }
  else
    {
      if (dest->cols <= src->cols && dest->rows <= src->rows)
        {
	  /* Downscale in both dimensions */
          image_scale_downsample_xy(dest, src);
	  return 1;
	}
      else
        {
	  image_scale_nearest_xy(dest, src);
	  return 1;
	}
    }
}

void
image_dimensions_fit_to_box(uns *cols, uns *rows, uns max_cols, uns max_rows, uns upsample)
{
  ASSERT(image_dimensions_valid(*cols, *rows));
  ASSERT(image_dimensions_valid(max_cols, max_rows));
  if (*cols <= max_cols && *rows <= max_rows)
    {
      if (!upsample)
	return;
      if (max_cols * *rows > max_rows * *cols)
        {
	  *cols = *cols * max_rows / *rows;
	  *cols = MIN(*cols, max_cols);
	  *rows = max_rows;
	}
      else
        {
	  *rows = *rows * max_cols / *cols;
	  *rows = MIN(*rows, max_rows);
	  *cols = max_cols;
	}
    }
  else if (*cols <= max_cols)
    goto down_cols;
  else if (*rows <= max_rows || max_rows * *cols > max_cols * *rows)
    goto down_rows;
down_cols:
  *cols = *cols * max_rows / *rows;
  *cols = MAX(*cols, 1);
  *rows = max_rows;
  return;
down_rows:
  *rows = *rows * max_cols / *cols;
  *rows = MAX(*rows, 1);
  *cols = max_cols;
}
