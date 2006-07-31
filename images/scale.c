/*
 *	Image Library -- Image scaling algorithms
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "images/images.h"
#include <string.h>

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

int
image_scale(struct image_thread *it, struct image *dest, struct image *src)
{
  if (src->cols < dest->cols || src->rows < dest->rows)
    {
      image_thread_err(it, IMAGE_ERR_INVALID_DIMENSIONS, "Upsampling not supported.");
      return 0;
    }
  if ((src->flags & IMAGE_PIXEL_FORMAT) != (dest->flags & IMAGE_PIXEL_FORMAT))
    {
      image_thread_err(it, IMAGE_ERR_INVALID_PIXEL_FORMAT, "Different pixel format not supported.");
      return 0;
    }
  switch (src->pixel_size)
    {
      /* Gray */
      case 1:
	image_scale_1_downsample(dest, src);
	return 1;
      /* GrayA */
      case 2:
	image_scale_2_downsample(dest, src);
	return 1;
      /* RGB */
      case 3:
	image_scale_3_downsample(dest, src);
	return 1;
      /* RGBA or aligned RGB */
      case 4:
	image_scale_4_downsample(dest, src);
	return 1;
      default:
	ASSERT(0);
    }
}

void
image_dimensions_fit_to_box(u32 *cols, u32 *rows, u32 max_cols, u32 max_rows, uns upsample)
{
  ASSERT(*cols && *rows && *cols <= IMAGE_MAX_SIZE && *rows <= IMAGE_MAX_SIZE);
  ASSERT(max_cols && max_rows && max_cols <= IMAGE_MAX_SIZE && max_rows <= IMAGE_MAX_SIZE);
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
