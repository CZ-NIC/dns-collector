/*
 *	Image Library -- Alpha channels
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "images/images.h"
#include "images/color.h"

static inline uns
merge_func(uns value, uns alpha, uns acoef, uns bcoef)
{
  return ((uns)(acoef + (int)alpha * (int)(value - bcoef)) * (0xffffffffU / 255 / 255)) >> 24;
}

int
image_apply_background(struct image_thread *thread UNUSED, struct image *dest, struct image *src, struct color *background)
{
  DBG("image_apply_background()");

  /* Grayscale */
  if (src->pixel_size == 2)
    {
      ASSERT(dest->pixel_size == 1);
      byte bg;
      if (background->color_space)
        color_put_grayscale(&bg, background);
      else
	bg = 0;
      uns a = 255 * bg, b = bg;
#     define IMAGE_WALK_PREFIX(x) walk_##x
#     define IMAGE_WALK_INLINE
#     define IMAGE_WALK_DOUBLE
#     define IMAGE_WALK_UNROLL 4
#     define IMAGE_WALK_IMAGE dest
#     define IMAGE_WALK_SEC_IMAGE src
#     define IMAGE_WALK_COL_STEP 1
#     define IMAGE_WALK_SEC_COL_STEP 2
#     define IMAGE_WALK_DO_STEP do{ walk_pos[0] = merge_func(walk_sec_pos[0], walk_sec_pos[1], a, b); }while(0)
#     include "images/image-walk.h"
    }

  /* RGBA to RGB or aligned RGB */
  else if (src->pixel_size == 4)
    {
      ASSERT((src->flags & IMAGE_ALPHA) && dest->pixel_size >= 3 && !(dest->flags & IMAGE_ALPHA));
      byte bg[3];
      if (background->color_space)
        color_put_rgb(bg, background);
      else
	bg[0] = bg[1] = bg[2] = 0;
      uns a0 = 255 * bg[0], b0 = bg[0];
      uns a1 = 255 * bg[1], b1 = bg[1];
      uns a2 = 255 * bg[2], b2 = bg[2];
#     define IMAGE_WALK_PREFIX(x) walk_##x
#     define IMAGE_WALK_INLINE
#     define IMAGE_WALK_DOUBLE
#     define IMAGE_WALK_UNROLL 2
#     define IMAGE_WALK_IMAGE dest
#     define IMAGE_WALK_SEC_IMAGE src
#     define IMAGE_WALK_SEC_COL_STEP 4
#     define IMAGE_WALK_DO_STEP do{ \
	  walk_pos[0] = merge_func(walk_sec_pos[0], walk_sec_pos[3], a0, b0); \
	  walk_pos[1] = merge_func(walk_sec_pos[1], walk_sec_pos[3], a1, b1); \
	  walk_pos[2] = merge_func(walk_sec_pos[2], walk_sec_pos[3], a2, b2); \
	}while(0)
#     include "images/image-walk.h"
    }
  else
    ASSERT(0);
  return 1;
}
