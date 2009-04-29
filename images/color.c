/*
 *	Image Library -- Color Spaces
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "images/images.h"
#include "images/color.h"
#include "images/error.h"
#include "images/math.h"

#include <string.h>
#include <math.h>

uns color_space_channels[COLOR_SPACE_MAX] = {
  [COLOR_SPACE_UNKNOWN] = 0,
  [COLOR_SPACE_UNKNOWN_1] = 1,
  [COLOR_SPACE_UNKNOWN_2] = 2,
  [COLOR_SPACE_UNKNOWN_3] = 3,
  [COLOR_SPACE_UNKNOWN_4] = 4,
  [COLOR_SPACE_GRAYSCALE] = 1,
  [COLOR_SPACE_RGB] = 3,
  [COLOR_SPACE_XYZ] = 3,
  [COLOR_SPACE_LAB] = 3,
  [COLOR_SPACE_YCBCR] = 3,
  [COLOR_SPACE_CMYK] = 4,
  [COLOR_SPACE_YCCK] = 4,
};

byte *color_space_name[COLOR_SPACE_MAX] = {
  [COLOR_SPACE_UNKNOWN] = "Unknown",
  [COLOR_SPACE_UNKNOWN_1] = "1-channel",
  [COLOR_SPACE_UNKNOWN_2] = "2-channels",
  [COLOR_SPACE_UNKNOWN_3] = "3-channels",
  [COLOR_SPACE_UNKNOWN_4] = "4-channels",
  [COLOR_SPACE_GRAYSCALE] = "Grayscale",
  [COLOR_SPACE_RGB] = "RGB",
  [COLOR_SPACE_XYZ] = "XYZ",
  [COLOR_SPACE_LAB] = "LAB",
  [COLOR_SPACE_YCBCR] = "YCbCr",
  [COLOR_SPACE_CMYK] = "CMYK",
  [COLOR_SPACE_YCCK] = "YCCK",
};

byte *
color_space_id_to_name(uns id)
{
  ASSERT(id < COLOR_SPACE_MAX);
  return color_space_name[id];
}

uns
color_space_name_to_id(byte *name)
{
  for (uns i = 1; i < COLOR_SPACE_MAX; i++)
    if (color_space_name[i] && !strcasecmp(name, color_space_name[i]))
      return i;
  return 0;
}

struct color color_black = { .color_space = COLOR_SPACE_GRAYSCALE };
struct color color_white = { .c = { 255 }, .color_space = COLOR_SPACE_GRAYSCALE };

int
color_get(struct color *color, byte *src, uns src_space)
{
  color->color_space = src_space;
  memcpy(color->c, src, color_space_channels[src_space]);
  return 1;
}

int
color_put(struct image_context *ctx, struct color *color, byte *dest, uns dest_space)
{
  switch (dest_space)
    {
      case COLOR_SPACE_GRAYSCALE:
	switch (color->color_space)
	  {
            case COLOR_SPACE_GRAYSCALE:
	      dest[0] = color->c[0];
	      return 1;
            case COLOR_SPACE_RGB:
	      dest[0] = rgb_to_gray_func(color->c[0], color->c[1], color->c[2]);
	      return 1;
	  }
	break;
      case COLOR_SPACE_RGB:
        switch (color->color_space)
          {
            case COLOR_SPACE_GRAYSCALE:
	      dest[0] = dest[1] = dest[2] = color->c[0];
	      return 1;
            case COLOR_SPACE_RGB:
	      dest[0] = color->c[0];
	      dest[1] = color->c[1];
	      dest[2] = color->c[2];
	      return 1;
	    case COLOR_SPACE_CMYK:
	      {
	        double rgb[3], cmyk[4];
	        for (uns i = 0; i < 4; i++)
		  cmyk[i] = color->c[i] * (1.0 / 255);
	        cmyk_to_rgb_exact(rgb, cmyk);
	        for (uns i = 0; i < 3; i++)
		  dest[i] = CLAMP(rgb[i] * 255, 0, 255);
	      }
	      return 1;
          }
	break;
      case COLOR_SPACE_CMYK:
	switch (color->color_space)
	  {
	    case COLOR_SPACE_GRAYSCALE:
	      dest[0] = dest[1] = dest[2] = 0;
	      dest[3] = 255 - color->c[0];
	      return 1;
	    case COLOR_SPACE_RGB:
	      {
	        double rgb[3], cmyk[4];
		for (uns i = 0; i < 3; i++)
		  rgb[i] = color->c[i] * (1.0 / 255);
		rgb_to_cmyk_exact(cmyk, rgb);
		for (uns i = 0; i < 4; i++)
		  dest[i] = CLAMP(cmyk[i] * 255, 0, 255);
	      }
	      return 1;
	  }
	break;
    }
  if (dest_space != COLOR_SPACE_RGB )
    {
      /* Try to convert the color via RGB */
      struct color rgb;
      if (!color_put(ctx, color, rgb.c, COLOR_SPACE_RGB))
	return 0;
      rgb.color_space = COLOR_SPACE_RGB;
      return color_put(ctx, &rgb, dest, dest_space);
    }
  IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Conversion from %s to %s is not supported",
      color_space_id_to_name(color->color_space), color_space_id_to_name(color->color_space));
  return 0;
}


/********************* IMAGE CONVERSION ROUTINES **********************/

struct image_conv_options image_conv_defaults = {
  .flags = IMAGE_CONV_COPY_ALPHA | IMAGE_CONV_FILL_ALPHA | IMAGE_CONV_APPLY_ALPHA,
  .background = { .color_space = COLOR_SPACE_GRAYSCALE } };

/* Grayscale <-> RGB */

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_gray_1_to_rgb_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_SEC_COL_STEP 1
#define IMAGE_WALK_UNROLL 4
#define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_pos[1] = walk_pos[2] = walk_sec_pos[0]; }while(0)
#include "images/image-walk.h"

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_rgb_n_to_gray_1
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_COL_STEP 1
#define IMAGE_WALK_UNROLL 2
#define IMAGE_WALK_DO_STEP do{ walk_pos[0] = rgb_to_gray_func(walk_sec_pos[0], walk_sec_pos[1], walk_sec_pos[2]); }while(0)
#include "images/image-walk.h"

/* Grayscale <-> YCbCr */

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_gray_1_to_ycbcr_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_SEC_COL_STEP 1
#define IMAGE_WALK_UNROLL 4
#define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; walk_pos[1] = walk_pos[2] = 0; }while(0)
#include "images/image-walk.h"

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_ycbcr_n_to_gray_1
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_COL_STEP 1
#define IMAGE_WALK_UNROLL 4
#define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; }while(0)
#include "images/image-walk.h"

/* YCbCr <-> RGB */

static inline void
pixel_conv_ycbcr_to_rgb(byte *dest, byte *src)
{
  /* R = Y                + 1.40200 * Cr
   * G = Y - 0.34414 * Cb - 0.71414 * Cr
   * B = Y + 1.77200 * Cb */
  int y = src[0], cb = src[1] - 128, cr = src[2] - 128;
  dest[0] = CLAMP(y + (91881 * cr) / 0x10000, 0, 255);
  dest[1] = CLAMP(y - (22553 * cb + 46801 * cr) / 0x10000, 0, 255);
  dest[2] = CLAMP(y + (116129 * cb) / 0x10000, 0, 255);
}

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_ycbcr_n_to_rgb_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_DO_STEP do{ pixel_conv_ycbcr_to_rgb(walk_pos, walk_sec_pos); }while(0)
#include "images/image-walk.h"

static inline void
pixel_conv_rgb_to_ycbcr(byte *dest, byte *src)
{
  /* Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
   * Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B + CENTER
   * Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B + CENTER */
  uns r = src[0], g = src[1], b = src[2];
  dest[0] = (19595 * r + 38470 * g + 7471 * b) / 0x10000;
  dest[1] = (0x800000 + 0x8000 * b - 11058 * r - 21710 * g) / 0x10000;
  dest[2] = (0x800000 + 0x8000 * r - 27439 * g - 5329 * b) / 0x10000;
}

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_rgb_n_to_ycbcr_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_DO_STEP do{ pixel_conv_rgb_to_ycbcr(walk_pos, walk_sec_pos); }while(0)
#include "images/image-walk.h"

/* CMYK <-> RGB */

static inline void
pixel_conv_cmyk_to_rgb(byte *dest, byte *src)
{
  uns d = (255 - src[3]) * (0xffffffffU / 255 /255);
  dest[0] = d * (255 - src[0]) >> 24;
  dest[1] = d * (255 - src[1]) >> 24;
  dest[2] = d * (255 - src[2]) >> 24;
}

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_cmyk_4_to_rgb_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_SEC_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_cmyk_to_rgb(walk_pos, walk_sec_pos); }while(0)
#include "images/image-walk.h"

static inline void
pixel_conv_rgb_to_cmyk(byte *dest, byte *src)
{
  uns k = MAX(src[0], src[1]);
  k = MAX(k, src[2]);
  uns d = fast_div_u32_u8(0x7fffffffU, k); /* == 0 for zero K */
  dest[0] = (d * (k - src[0])) >> 23;
  dest[1] = (d * (k - src[1])) >> 23;
  dest[2] = (d * (k - src[2])) >> 23;
  dest[3] = 255 - k;
}

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_rgb_n_to_cmyk_4
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_rgb_to_cmyk(walk_pos, walk_sec_pos); }while(0)
#include "images/image-walk.h"

/* CMYK <-> YCbCr */

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_cmyk_4_to_ycbcr_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_SEC_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_cmyk_to_rgb(walk_pos, walk_sec_pos); pixel_conv_rgb_to_ycbcr(walk_pos, walk_pos); }while(0)
#include "images/image-walk.h"

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_ycbcr_n_to_cmyk_4
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_ycbcr_to_rgb(walk_pos, walk_sec_pos); pixel_conv_rgb_to_cmyk(walk_pos, walk_pos); }while(0)
#include "images/image-walk.h"

/* YCCK <-> RGB */

static inline void
pixel_conv_ycck_to_rgb(byte *dest, byte *src)
{
  int y = src[0], cb = src[1] - 128, cr = src[2] - 128;
  uns d = (255 - src[3]) * (0xffffffffU / 255 /255);
  dest[0] = (d * CLAMP(y + (91881 * cr) / 0x10000, 0, 255) >> 24);
  dest[1] = (d * CLAMP(y - (22553 * cb + 46801 * cr) / 0x10000, 0, 255) >> 24);
  dest[2] = (d * CLAMP(y + (116129 * cb) / 0x10000, 0, 255) >> 24);
}

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_ycck_4_to_rgb_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_SEC_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_ycck_to_rgb(walk_pos, walk_sec_pos); }while(0)
#include "images/image-walk.h"

static inline void
pixel_conv_rgb_to_ycck(byte *dest, byte *src)
{
  uns k = MAX(src[0], src[1]);
  k = MAX(k, src[2]);
  uns d = fast_div_u32_u8(0x7fffffffU, k); /* == 0 for zero K */
  uns r = 255 - ((d * (k - src[0])) >> 23);
  uns g = 255 - ((d * (k - src[1])) >> 23);
  uns b = 255 - ((d * (k - src[2])) >> 23);
  dest[0] = (19595 * r + 38470 * g + 7471 * b) / 0x10000;
  dest[1] = (0x800000 + 0x8000 * b - 11058 * r - 21710 * g) / 0x10000;
  dest[2] = (0x800000 + 0x8000 * r - 27439 * g - 5329 * b) / 0x10000;
  dest[3] = 255 - k;
}

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_rgb_n_to_ycck_4
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_rgb_to_ycck(walk_pos, walk_sec_pos); }while(0)
#include "images/image-walk.h"

/* YCCK <-> RGB */

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_ycck_4_to_ycbcr_n
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_SEC_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_ycck_to_rgb(walk_pos, walk_sec_pos); pixel_conv_rgb_to_ycbcr(walk_pos, walk_pos); }while(0)
#include "images/image-walk.h"

#define IMAGE_WALK_PREFIX(x) walk_##x
#define IMAGE_WALK_FUNC_NAME image_conv_ycbcr_n_to_ycck_4
#define IMAGE_WALK_DOUBLE
#define IMAGE_WALK_COL_STEP 4
#define IMAGE_WALK_DO_STEP do{ pixel_conv_ycbcr_to_rgb(walk_pos, walk_sec_pos); pixel_conv_rgb_to_ycck(walk_pos, walk_pos); }while(0)
#include "images/image-walk.h"

/* Main functions */

static int
image_conv_color_space(struct image_context *ctx UNUSED, struct image *dest, struct image *src, struct image_conv_options *opt UNUSED)
{
  switch (dest->flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
        switch (src->flags & IMAGE_COLOR_SPACE)
          {
	    case COLOR_SPACE_RGB:
	      if (dest->pixel_size == 1)
	        {
	          image_conv_rgb_n_to_gray_1(dest, src);
	          return 1;
	        }
	      break;
            case COLOR_SPACE_YCBCR:
              if (dest->pixel_size == 1)
		{
	          image_conv_ycbcr_n_to_gray_1(dest, src);
	          return 1;
		}
	      break;
	  }
	break;
      case COLOR_SPACE_RGB:
	switch (src->flags & IMAGE_CHANNELS_FORMAT)
	  {
	    case COLOR_SPACE_GRAYSCALE:
	      if (src->pixel_size == 1)
	        {
	          image_conv_gray_1_to_rgb_n(dest, src);
	          return 1;
	        }
	      break;
	    case COLOR_SPACE_YCBCR:
	      image_conv_ycbcr_n_to_rgb_n(dest, src);
	      return 1;
	    case COLOR_SPACE_CMYK:
	      if (src->pixel_size == 4)
	        {
	          image_conv_cmyk_4_to_rgb_n(dest, src);
	          return 1;
	        }
	      break;
	    case COLOR_SPACE_YCCK:
	      if (src->pixel_size == 4)
	        {
		  image_conv_ycck_4_to_rgb_n(dest, src);
		  return 1;
		}
	      break;
	}
	break;
      case COLOR_SPACE_YCBCR:
        switch (src->flags & IMAGE_CHANNELS_FORMAT)
	  {
            case COLOR_SPACE_GRAYSCALE:
              if (src->pixel_size == 1)
		{
                  image_conv_gray_1_to_ycbcr_n(dest, src);
		  return 1;
		}
	      break;
	    case COLOR_SPACE_RGB:
	      image_conv_rgb_n_to_ycbcr_n(dest, src);
	      return 1;
	    case COLOR_SPACE_CMYK:
	      if (src->pixel_size == 4)
	        {
	          image_conv_cmyk_4_to_ycbcr_n(dest, src);
	          return 1;
	        }
	      break;
	    case COLOR_SPACE_YCCK:
	      if (src->pixel_size == 4)
	        {
		  image_conv_ycck_4_to_ycbcr_n(dest, src);
		  return 1;
		}
	      break;
	  }
	break;
      case COLOR_SPACE_CMYK:
        switch (src->flags & IMAGE_CHANNELS_FORMAT)
          {
	    case COLOR_SPACE_RGB:
	      if (dest->pixel_size == 4)
	        {
	          image_conv_rgb_n_to_cmyk_4(dest, src);
	          return 1;
	        }
	      break;
	    case COLOR_SPACE_YCBCR:
	      if (dest->pixel_size == 4)
	        {
	          image_conv_ycbcr_n_to_cmyk_4(dest, src);
	          return 1;
	        }
	      break;
          }
        break;
      case COLOR_SPACE_YCCK:
	switch (src->flags & IMAGE_CHANNELS_FORMAT)
	  {
	    case COLOR_SPACE_RGB:
	      if (dest->pixel_size == 4)
	        {
		  image_conv_rgb_n_to_ycck_4(dest, src);
		  return 1;
		}
	      break;
	    case COLOR_SPACE_YCBCR:
	      if (dest->pixel_size == 4)
	        {
		  image_conv_ycbcr_n_to_ycck_4(dest, src);
		  return 1;
		}
	      break;
	  }
	break;
    }
  return 0;
}

static void
image_conv_copy(struct image *dest, struct image *src)
{
  if (dest->pixels == src->pixels)
    return;
  else if (dest->pixel_size != src->pixel_size)
    {
      uns channels = MIN(dest->channels, src->channels);
      switch (channels)
        {
	  case 1:
	    {
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#	      define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_DOUBLE
#             define IMAGE_WALK_IMAGE dest
#             define IMAGE_WALK_SEC_IMAGE src
#	      define IMAGE_WALK_UNROLL 4
#	      define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; }while(0)
#	      include "images/image-walk.h"
            }
	    return;
	  case 2:
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#	      define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_DOUBLE
#             define IMAGE_WALK_IMAGE dest
#             define IMAGE_WALK_SEC_IMAGE src
#	      define IMAGE_WALK_UNROLL 4
#	      define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; walk_pos[1] = walk_sec_pos[1]; }while(0)
#	      include "images/image-walk.h"
	    return;
	  case 3:
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#	      define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_DOUBLE
#             define IMAGE_WALK_IMAGE dest
#             define IMAGE_WALK_SEC_IMAGE src
#	      define IMAGE_WALK_UNROLL 2
#	      define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; walk_pos[1] = walk_sec_pos[1]; walk_pos[2] = walk_sec_pos[2]; }while(0)
#	      include "images/image-walk.h"
	    return;
	  case 4:
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#	      define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_DOUBLE
#             define IMAGE_WALK_IMAGE dest
#             define IMAGE_WALK_SEC_IMAGE src
#	      define IMAGE_WALK_UNROLL 2
#	      define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; walk_pos[1] = walk_sec_pos[1]; walk_pos[2] = walk_sec_pos[2]; walk_pos[3] = walk_sec_pos[3]; }while(0)
#	      include "images/image-walk.h"
	    return;
	  default:
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#	      define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_DOUBLE
#             define IMAGE_WALK_IMAGE dest
#             define IMAGE_WALK_SEC_IMAGE src
#	      define IMAGE_WALK_DO_STEP do{ for (uns i = 0; i < channels; i++) walk_pos[i] = walk_sec_pos[i]; }while(0)
#             include "images/image-walk.h"
	    return;
	}
    }
  else if (dest->row_size != src->row_size || ((dest->flags | src->flags) & IMAGE_GAPS_PROTECTED))
    {
      byte *s = src->pixels;
      byte *d = dest->pixels;
      for (uns row = src->rows; row--; )
        {
          memcpy(d, s, src->row_pixels_size);
          d += dest->row_size;
          s += src->row_size;
        }
    }
  else if (dest->pixels != src->pixels)
    memcpy(dest->pixels, src->pixels, src->image_size);
}

static void
image_conv_fill_alpha(struct image *dest)
{
  switch (dest->channels)
    {
      case 2:
	if (dest->pixel_size == 2)
	  {
#           define IMAGE_WALK_PREFIX(x) walk_##x
#           define IMAGE_WALK_INLINE
#           define IMAGE_WALK_IMAGE dest
#           define IMAGE_WALK_COL_STEP 2
#           define IMAGE_WALK_UNROLL 4
#           define IMAGE_WALK_DO_STEP do{ walk_pos[1] = 255; }while(0)
#           include "images/image-walk.h"
	    return;
	  }
	break;
      case 4:
	if (dest->pixel_size == 4)
	  {
#           define IMAGE_WALK_PREFIX(x) walk_##x
#           define IMAGE_WALK_INLINE
#           define IMAGE_WALK_IMAGE dest
#           define IMAGE_WALK_COL_STEP 4
#           define IMAGE_WALK_UNROLL 4
#           define IMAGE_WALK_DO_STEP do{ walk_pos[3] = 255; }while(0)
#           include "images/image-walk.h"
	    return;
	  }
	break;
    }
  {
#   define IMAGE_WALK_PREFIX(x) walk_##x
#   define IMAGE_WALK_INLINE
#   define IMAGE_WALK_IMAGE dest
#   define IMAGE_WALK_UNROLL 4
#   define IMAGE_WALK_DO_STEP do{ walk_pos[dest->channels - 1] = 255; }while(0)
#   include "images/image-walk.h"
  }
}

static void
image_conv_copy_alpha(struct image *dest, struct image *src)
{
  if (dest->pixels != src->pixels || dest->channels != src->channels)
    {
#     define IMAGE_WALK_PREFIX(x) walk_##x
#     define IMAGE_WALK_INLINE
#     define IMAGE_WALK_DOUBLE
#     define IMAGE_WALK_IMAGE dest
#     define IMAGE_WALK_SEC_IMAGE src
#     define IMAGE_WALK_UNROLL 4
#     define IMAGE_WALK_DO_STEP do{ walk_pos[dest->channels - 1] = walk_sec_pos[src->channels - 1]; }while(0)
#     include "images/image-walk.h"
    }
}

static inline uns
image_conv_alpha_func(uns value, uns alpha, uns acoef, uns bcoef)
{
  return ((uns)(acoef + (int)alpha * (int)(value - bcoef)) * (0xffffffffU / 255 / 255)) >> 24;
}

static int
image_conv_apply_alpha_from(struct image_context *ctx, struct image *dest, struct image *src, struct image_conv_options *opt)
{
  if (!opt->background.color_space)
    return 1;
  byte background[IMAGE_MAX_CHANNELS];
  if (unlikely(!color_put(ctx, &opt->background, background, dest->flags & IMAGE_COLOR_SPACE)))
    return 0;
  uns a[IMAGE_MAX_CHANNELS], b[IMAGE_MAX_CHANNELS];
  for (uns i = 0; i < dest->channels; i++)
    a[i] = 255 * (b[i] = background[i]);
  switch (dest->channels)
    {
      case 1:
	{
#         define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#         define IMAGE_WALK_IMAGE dest
#         define IMAGE_WALK_SEC_IMAGE src
#         define IMAGE_WALK_DOUBLE
#	  define IMAGE_WALK_UNROLL 2
#         define IMAGE_WALK_DO_STEP do{ \
              walk_pos[0] = image_conv_alpha_func(walk_pos[0], walk_sec_pos[src->channels - 1], a[0], b[0]); }while(0)
#         include "images/image-walk.h"
	}
	return 1;
      case 3:
	{
#         define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#         define IMAGE_WALK_IMAGE dest
#         define IMAGE_WALK_SEC_IMAGE src
#         define IMAGE_WALK_DOUBLE
#         define IMAGE_WALK_DO_STEP do{ \
              walk_pos[0] = image_conv_alpha_func(walk_pos[0], walk_sec_pos[src->channels - 1], a[0], b[0]); \
              walk_pos[1] = image_conv_alpha_func(walk_pos[1], walk_sec_pos[src->channels - 1], a[1], b[1]); \
              walk_pos[2] = image_conv_alpha_func(walk_pos[2], walk_sec_pos[src->channels - 1], a[2], b[2]); }while(0)
#         include "images/image-walk.h"
	}
	return 1;
    }
  {
#   define IMAGE_WALK_PREFIX(x) walk_##x
#   define IMAGE_WALK_INLINE
#   define IMAGE_WALK_IMAGE dest
#   define IMAGE_WALK_SEC_IMAGE src
#   define IMAGE_WALK_DOUBLE
#   define IMAGE_WALK_DO_STEP do{ for (uns i = 0; i < dest->channels; i++) \
        walk_pos[i] = image_conv_alpha_func(walk_pos[i], walk_sec_pos[src->channels - 1], a[i], b[i]); }while(0)
#   include "images/image-walk.h"
  }
  return 1;
}

static int
image_conv_apply_alpha_to(struct image_context *ctx, struct image *dest, struct image *src, struct image_conv_options *opt)
{
  if (!opt->background.color_space)
    {
      image_conv_copy(dest, src);
      return 1;
    }
  byte background[IMAGE_MAX_CHANNELS];
  if (unlikely(!color_put(ctx, &opt->background, background, dest->flags & IMAGE_COLOR_SPACE)))
    return 0;
  uns a[IMAGE_MAX_CHANNELS], b[IMAGE_MAX_CHANNELS];
  for (uns i = 0; i < dest->channels; i++)
    a[i] = 255 * (b[i] = background[i]);
  switch (dest->channels)
    {
      case 1:
	{
#         define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#         define IMAGE_WALK_IMAGE dest
#         define IMAGE_WALK_SEC_IMAGE src
#         define IMAGE_WALK_DOUBLE
#	  define IMAGE_WALK_UNROLL 2
#         define IMAGE_WALK_DO_STEP do{ \
              walk_pos[0] = image_conv_alpha_func(walk_sec_pos[0], walk_sec_pos[src->channels - 1], a[0], b[0]); }while(0)
#         include "images/image-walk.h"
	}
	return 1;
      case 3:
	{
#         define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#         define IMAGE_WALK_IMAGE dest
#         define IMAGE_WALK_SEC_IMAGE src
#         define IMAGE_WALK_DOUBLE
#         define IMAGE_WALK_DO_STEP do{ \
              walk_pos[0] = image_conv_alpha_func(walk_sec_pos[0], walk_sec_pos[src->channels - 1], a[0], b[0]); \
              walk_pos[1] = image_conv_alpha_func(walk_sec_pos[1], walk_sec_pos[src->channels - 1], a[1], b[1]); \
              walk_pos[2] = image_conv_alpha_func(walk_sec_pos[2], walk_sec_pos[src->channels - 1], a[2], b[2]); }while(0)
#         include "images/image-walk.h"
	}
	return 1;
    }
  {
#   define IMAGE_WALK_PREFIX(x) walk_##x
#   define IMAGE_WALK_INLINE
#   define IMAGE_WALK_IMAGE dest
#   define IMAGE_WALK_SEC_IMAGE src
#   define IMAGE_WALK_DOUBLE
#   define IMAGE_WALK_DO_STEP do{ for (uns i = 0; i < dest->channels; i++) \
        walk_pos[i] = image_conv_alpha_func(walk_sec_pos[i], walk_sec_pos[src->channels - 1], a[i], b[i]); }while(0)
#   include "images/image-walk.h"
  }
  return 1;
}

int
image_conv(struct image_context *ctx, struct image *dest, struct image *src, struct image_conv_options *opt)
{
  ASSERT(dest->cols == src->cols && dest->rows == src->rows);
  if (!((dest->flags ^ src->flags) & IMAGE_COLOR_SPACE))
    {
      if (!(src->flags & IMAGE_ALPHA) || (dest->flags & IMAGE_ALPHA))
	image_conv_copy(dest, src);
      else if (unlikely(!image_conv_apply_alpha_to(ctx, dest, src, opt)))
	return 0;
    }
  else
    {
      if (!(src->flags & IMAGE_ALPHA))
        {
	  if (unlikely(!image_conv_color_space(ctx, dest, src, opt)))
	    goto error;
	  if ((dest->flags & IMAGE_ALPHA) && (opt->flags & IMAGE_CONV_FILL_ALPHA))
	    image_conv_fill_alpha(dest);
	}
      else
        {
	  if (dest->flags & IMAGE_ALPHA)
	    {
	      if (dest->channels <= src->channels)
	        {
	          if (unlikely(!image_conv_color_space(ctx, dest, src, opt)))
	            goto error;
		  if (opt->flags & IMAGE_CONV_COPY_ALPHA)
		    image_conv_copy_alpha(dest, src);
		  else if (opt->flags & IMAGE_CONV_FILL_ALPHA)
		    image_conv_fill_alpha(dest);
		}
	      else
	        {
		  if (opt->flags & IMAGE_CONV_COPY_ALPHA)
		    image_conv_copy_alpha(dest, src);
		  else
		    image_conv_fill_alpha(dest);
	          if (unlikely(!image_conv_color_space(ctx, dest, src, opt)))
	            goto error;
		}
	    }
	  else
	    {
	      if (dest->channels <= src->channels)
	        {
	          if (unlikely(!image_conv_color_space(ctx, dest, src, opt)))
	            goto error;
		  if (unlikely(!image_conv_apply_alpha_from(ctx, dest, src, opt)))
		    return 0;
		}
	      else
	        {
		  if (unlikely(!image_conv_apply_alpha_to(ctx, dest, src, opt)))
		    return 0;
	          if (unlikely(!image_conv_color_space(ctx, dest, dest, opt)))
	            goto error;
		}
	    }
	}
    }
  return 1;
error:
  IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Image conversion not supported for such pixel formats");
  return 0;
}

/********************* EXACT CONVERSION ROUTINES **********************/

/* Reference whites */
#define COLOR_ILLUMINANT_A	0.44757, 0.40744
#define COLOR_ILLUMINANT_B	0.34840, 0.35160
#define COLOR_ILLUMINANT_C	0.31006, 0.31615
#define COLOR_ILLUMINANT_D50    0.34567, 0.35850
#define COLOR_ILLUMINANT_D55	0.33242, 0.34743
#define COLOR_ILLUMINANT_D65    0.31273, 0.32902
#define COLOR_ILLUMINANT_D75	0.29902, 0.31485
#define COLOR_ILLUMINANT_9300K	0.28480, 0.29320
#define COLOR_ILLUMINANT_E      (1./3.), (1./3.)
#define COLOR_ILLUMINANT_F2	0.37207, 0.37512
#define COLOR_ILLUMINANT_F7	0.31285, 0.32918
#define COLOR_ILLUMINANT_F11	0.38054, 0.37691

const double
  color_illuminant_d50[2] = {COLOR_ILLUMINANT_D50},
  color_illuminant_d65[2] = {COLOR_ILLUMINANT_D65},
  color_illuminant_e[2] = {COLOR_ILLUMINANT_E};

/* RGB profiles (many missing) */
const struct color_space_info
  color_adobe_rgb_info = {"Adobe RGB", {{0.6400, 0.3300}, {0.2100, 0.7100}, {0.1500, 0.0600}, {COLOR_ILLUMINANT_D65}}, {0.45, 0.45, 0, 0, 0}},
  color_apple_rgb_info = {"Apple RGB", {{0.6250, 0.3400}, {0.2800, 0.5950}, {0.1550, 0.0700}, {COLOR_ILLUMINANT_D65}}, {0.56, 0.56, 0, 0, 0}},
  color_cie_rgb_info = {"CIE RGB", {{0.7350, 0.2650}, {0.2740, 0.7170}, {0.1670, 0.0090}, {COLOR_ILLUMINANT_E}}, {0.45, 0.45, 0, 0, 0}},
  color_color_match_rgb_info = {"ColorMatch RGB", {{0.6300, 0.3400}, {0.2950, 0.6050}, {0.1500, 0.0750}, {COLOR_ILLUMINANT_D50}}, {0.56, 0.56, 0, 0, 0}},
  color_srgb_info = {"sRGB", {{0.6400, 0.3300}, {0.3000, 0.6000}, {0.1500, 0.0600}, {COLOR_ILLUMINANT_D65}}, {0.45, 0.42, 0.055, 0.003, 12.92}};

#define CLIP(x, min, max) (((x) < (min)) ? (min) : ((x) > (max)) ? (max) : (x))

static inline void
clip(double a[3])
{
  a[0] = CLIP(a[0], 0, 1);
  a[1] = CLIP(a[1], 0, 1);
  a[2] = CLIP(a[2], 0, 1);
}

static inline void
correct_gamma_simple(double dest[3], double src[3], const struct color_space_gamma_info *info)
{
  dest[0] = pow(src[0], info->simple_gamma);
  dest[1] = pow(src[1], info->simple_gamma);
  dest[2] = pow(src[2], info->simple_gamma);
}

static inline void
invert_gamma_simple(double dest[3], double src[3], const struct color_space_gamma_info *info)
{
  dest[0] = pow(src[0], 1 / info->simple_gamma);
  dest[1] = pow(src[1], 1 / info->simple_gamma);
  dest[2] = pow(src[2], 1 / info->simple_gamma);
}

static inline void
correct_gamma_detailed(double dest[3], double src[3], const struct color_space_gamma_info *info)
{
  for (uns i = 0; i < 3; i++)
    if (src[i] > info->transition)
      dest[i] = (1 + info->offset) * pow(src[i], info->detailed_gamma) - info->offset;
    else
      dest[i] = info->slope * src[i];
}

static inline void
invert_gamma_detailed(double dest[3], double src[3], const struct color_space_gamma_info *info)
{
  for (uns i = 0; i < 3; i++)
    if (src[i] > info->transition * info->slope)
      dest[i] = pow((src[i] + info->offset) / (1 + info->offset), 1 / info->detailed_gamma);
    else
      dest[i] = src[i] / info->slope;
}

static inline void
apply_matrix(double dest[3], double src[3], double matrix[9])
{
  dest[0] = src[0] * matrix[0] + src[1] * matrix[1] + src[2] * matrix[2];
  dest[1] = src[0] * matrix[3] + src[1] * matrix[4] + src[2] * matrix[5];
  dest[2] = src[0] * matrix[6] + src[1] * matrix[7] + src[2] * matrix[8];
}

void
color_invert_matrix(double dest[9], double matrix[9])
{
  double *i = dest, *m = matrix;
  double a0 = m[4] * m[8] - m[5] * m[7];
  double a1 = m[3] * m[8] - m[5] * m[6];
  double a2 = m[3] * m[7] - m[4] * m[6];
  double d = 1 / (m[0] * a0 - m[1] * a1 + m[2] * a2);
  i[0] = d * a0;
  i[3] = -d * a1;
  i[6] = d * a2;
  i[1] = -d * (m[1] * m[8] - m[2] * m[7]);
  i[4] = d * (m[0] * m[8] - m[2] * m[6]);
  i[7] = -d * (m[0] * m[7] - m[1] * m[6]);
  i[2] = d * (m[1] * m[5] - m[2] * m[4]);
  i[5] = -d * (m[0] * m[5] - m[2] * m[3]);
  i[8] = d * (m[0] * m[4] - m[1] * m[3]);
}

static void
mul_matrices(double r[9], double a[9], double b[9])
{
  r[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
  r[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
  r[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
  r[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
  r[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
  r[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];
  r[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
  r[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
  r[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
}

/* computes conversion matrix from a given color space to CIE XYZ */
void
color_compute_color_space_to_xyz_matrix(double matrix[9], const struct color_space_chromacity_info *space)
{
  double wX = space->white[0] / space->white[1];
  double wZ = (1 - space->white[0] - space->white[1]) / space->white[1];
  double a[9], b[9];
  a[0] = space->prim1[0]; a[3] = space->prim1[1]; a[6] = 1 - a[0] - a[3];
  a[1] = space->prim2[0]; a[4] = space->prim2[1]; a[7] = 1 - a[1] - a[4];
  a[2] = space->prim3[0]; a[5] = space->prim3[1]; a[8] = 1 - a[2] - a[5];
  color_invert_matrix(b, a);
  double ra = wX * b[0] + b[1] + wZ * b[2];
  double rb = wX * b[3] + b[4] + wZ * b[5];
  double rc = wX * b[6] + b[7] + wZ * b[8];
  matrix[0] = a[0] * ra;
  matrix[1] = a[1] * rb;
  matrix[2] = a[2] * rc;
  matrix[3] = a[3] * ra;
  matrix[4] = a[4] * rb;
  matrix[5] = a[5] * rc;
  matrix[6] = a[6] * ra;
  matrix[7] = a[7] * rb;
  matrix[8] = a[8] * rc;
}

/* computes matrix to join transformations with different reference whites */
void
color_compute_bradford_matrix(double matrix[9], const double source[2], const double dest[2])
{
  /* cone response matrix and its inversion */
  static double r[9] = {
    0.8951, 0.2664, -0.1614,
    -0.7502, 1.7135, 0.0367,
    0.0389, -0.0685, 1.0296};
  //static double i[9] = {0.9870, -0.1471, 0.1600, 0.4323, 0.5184, 0.0493, -0.0085, 0.0400, 0.9685};
  double i[9];
  color_invert_matrix(i, r);
  double aX = source[0] / source[1];
  double aZ = (1 - source[0] - source[1]) / source[1];
  double bX = dest[0] / dest[1];
  double bZ = (1 - dest[0] - dest[1]) / dest[1];
  double x = (r[0] * bX + r[1] + r[2] * bZ) / (r[0] * aX + r[1] + r[2] * aZ);
  double y = (r[3] * bX + r[4] + r[5] * bZ) / (r[3] * aX + r[4] + r[5] * aZ);
  double z = (r[6] * bX + r[7] + r[8] * bZ) / (r[6] * aX + r[7] + r[8] * aZ);
  double m[9];
  m[0] = i[0] * x; m[1] = i[1] * y; m[2] = i[2] * z;
  m[3] = i[3] * x; m[4] = i[4] * y; m[5] = i[5] * z;
  m[6] = i[6] * x; m[7] = i[7] * y; m[8] = i[8] * z;
  mul_matrices(matrix, m, r);
}

void
color_compute_color_spaces_conversion_matrix(double matrix[9], const struct color_space_chromacity_info *src, const struct color_space_chromacity_info *dest)
{
  double a_to_xyz[9], b_to_xyz[9], xyz_to_b[9], bradford[9], m[9];
  color_compute_color_space_to_xyz_matrix(a_to_xyz, src);
  color_compute_color_space_to_xyz_matrix(b_to_xyz, dest);
  color_invert_matrix(xyz_to_b, b_to_xyz);
  if (src->white[0] == dest->white[0] && src->white[1] == dest->white[1])
    mul_matrices(matrix, a_to_xyz, xyz_to_b);
  else
    {
      color_compute_bradford_matrix(bradford, src->white, dest->white);
      mul_matrices(m, a_to_xyz, bradford);
      mul_matrices(matrix, m, xyz_to_b);
    }
}

/* sRGB to XYZ */
void
srgb_to_xyz_exact(double xyz[3], double srgb[3])
{
  static double matrix[9] = {
    0.41248031, 0.35756952, 0.18043951,
    0.21268516, 0.71513904, 0.07217580,
    0.01933501, 0.11918984, 0.95031473};
  double srgb_lin[3];
  invert_gamma_detailed(srgb_lin, srgb, &color_srgb_info.gamma);
  apply_matrix(xyz, srgb_lin, matrix);
  xyz_to_srgb_exact(srgb_lin, xyz);
}

/* XYZ to sRGB */
void
xyz_to_srgb_exact(double srgb[3], double xyz[3])
{
  static double matrix[9] = {
     3.24026666, -1.53704957, -0.49850256,
    -0.96928381,  1.87604525,  0.04155678,
     0.05564281, -0.20402363,  1.05721334};
  double srgb_lin[3];
  apply_matrix(srgb_lin, xyz, matrix);
  clip(srgb_lin);
  correct_gamma_detailed(srgb, srgb_lin, &color_srgb_info.gamma);
}

/* XYZ to CIE-Luv */
void
xyz_to_luv_exact(double luv[3], double xyz[3])
{
  double sum = xyz[0] + 15 * xyz[1] + 3 * xyz[2];
  if (sum < 0.000001)
    luv[0] = luv[1] = luv[2] = 0;
  else
    {
      double var_u = 4 * xyz[0] / sum;
      double var_v = 9 * xyz[1] / sum;
      if (xyz[1] > 0.008856)
        luv[0] = 116 * pow(xyz[1], 1 / 3.) - 16;
      else
        luv[0] = (116 * 7.787) * xyz[1];
     luv[1] = luv[0] * (13 * (var_u - 4 * REF_WHITE_X / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z)));
     luv[2] = luv[0] * (13 * (var_v - 9 * REF_WHITE_Y / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z)));
     /* intervals [0..100], [-134..220], [-140..122] */
   }
}

/* CIE-Luv to XYZ */
void
luv_to_xyz_exact(double xyz[3], double luv[3])
{
  double var_u = luv[1] / (13 * luv[0]) + (4 * REF_WHITE_X / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z));
  double var_v = luv[2] / (13 * luv[0]) + (9 * REF_WHITE_Y / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z));
  double var_y = (luv[0] + 16) / 116;
  double pow_y = var_y * var_y * var_y;
  if (pow_y > 0.008856)
    var_y = pow_y;
  else
    var_y = (var_y - 16 / 116) / 7.787;
  xyz[1] = var_y;
  xyz[0] = -(9 * xyz[1] * var_u) / ((var_u - 4) * var_v - var_u * var_v);
  xyz[2] = (9 * xyz[1] - 15 * var_v * xyz[1] - var_v * xyz[0]) / (3 * var_v);
}

/* RGB to CMYK - a very simple version, not too accureate  */
void
rgb_to_cmyk_exact(double cmyk[4], double rgb[3])
{
  cmyk[0] = 1 - rgb[0];
  cmyk[1] = 1 - rgb[1];
  cmyk[2] = 1 - rgb[2];
  cmyk[3] = MIN(cmyk[0], cmyk[1]);
  cmyk[3] = MIN(cmyk[3], cmyk[2]);
  if (cmyk[3] > 0.9999)
    {
      cmyk[3] = 1;
      cmyk[0] = cmyk[1] = cmyk[2] = 0;
    }
  else
    {
      double d = 1 / (1 - cmyk[3]);
      for (uns i = 0; i < 3; i++)
        cmyk[i] = d * (cmyk[i] - cmyk[3]);
    }
}

/* CMYK to RGB */
void
cmyk_to_rgb_exact(double rgb[3], double cmyk[4])
{
  double d = 1 - cmyk[1];
  for (uns i = 0; i < 3; i++)
    rgb[i] = d * (1 - cmyk[i]);
}

/***************** OPTIMIZED SRGB -> LUV CONVERSION *********************/

u16 srgb_to_luv_tab1[256];
u16 srgb_to_luv_tab2[9 << SRGB_TO_LUV_TAB2_SIZE];
u32 srgb_to_luv_tab3[20 << SRGB_TO_LUV_TAB3_SIZE];

void
srgb_to_luv_init(void)
{
  DBG("Initializing sRGB -> Luv table");
  for (uns i = 0; i < 256; i++)
    {
      double t = i / 255.;
      if (t > 0.04045)
        t = pow((t + 0.055) * (1 / 1.055), 2.4);
      else
        t = t * (1 / 12.92);
      srgb_to_luv_tab1[i] = CLAMP(t * 0xfff + 0.5, 0, 0xfff);
    }
  for (uns i = 0; i < (9 << SRGB_TO_LUV_TAB2_SIZE); i++)
    {
      double t = i / (double)((9 << SRGB_TO_LUV_TAB2_SIZE) - 1);
      if (t > 0.008856)
        t = 1.16 * pow(t, 1 / 3.) - 0.16;
      else
        t = (1.16 * 7.787) * t;
      srgb_to_luv_tab2[i] =
	CLAMP(t * ((1 << SRGB_TO_LUV_TAB2_SCALE) - 1) + 0.5,
          0, (1 << SRGB_TO_LUV_TAB2_SCALE) - 1);
    }
  for (uns i = 0; i < (20 << SRGB_TO_LUV_TAB3_SIZE); i++)
    {
      srgb_to_luv_tab3[i] = i ? (13 << (SRGB_TO_LUV_TAB3_SCALE + SRGB_TO_LUV_TAB3_SIZE)) / i : 0;
    }
}

void
srgb_to_luv_pixels(byte *dest, byte *src, uns count)
{
  while (count--)
    {
      srgb_to_luv_pixel(dest, src);
      dest += 3;
      src += 3;
    }
}


/************************ GRID INTERPOLATION ALGORITHM ************************/

struct color_grid_node *srgb_to_luv_grid;
struct color_interpolation_node *color_interpolation_table;

/* Returns volume of a given tetrahedron multiplied by 6 */
static inline uns
tetrahedron_volume(uns *v1, uns *v2, uns *v3, uns *v4)
{
  int a[3], b[3], c[3];
  for (uns i = 0; i < 3; i++)
    {
      a[i] = v2[i] - v1[i];
      b[i] = v3[i] - v1[i];
      c[i] = v4[i] - v1[i];
    }
  int result =
    a[0] * (b[1] * c[2] - b[2] * c[1]) -
    a[1] * (b[0] * c[2] - b[2] * c[0]) +
    a[2] * (b[0] * c[1] - b[1] * c[0]);
  return (result > 0) ? result : -result;
}

static void
interpolate_tetrahedron(struct color_interpolation_node *n, uns *p, const uns *c)
{
  uns v[4][3];
  for (uns i = 0; i < 4; i++)
    {
      v[i][0] = (c[i] & 0001) ? (1 << COLOR_CONV_OFS) : 0;
      v[i][1] = (c[i] & 0010) ? (1 << COLOR_CONV_OFS) : 0;
      v[i][2] = (c[i] & 0100) ? (1 << COLOR_CONV_OFS) : 0;
      n->ofs[i] =
	((c[i] & 0001) ? 1 : 0) +
	((c[i] & 0010) ? (1 << COLOR_CONV_SIZE) : 0) +
	((c[i] & 0100) ? (1 << (COLOR_CONV_SIZE * 2)) : 0);
    }
  uns vol = tetrahedron_volume(v[0], v[1], v[2], v[3]);
  n->mul[0] = ((tetrahedron_volume(p, v[1], v[2], v[3]) << 8) + (vol >> 1)) / vol;
  n->mul[1] = ((tetrahedron_volume(v[0], p, v[2], v[3]) << 8) + (vol >> 1)) / vol;
  n->mul[2] = ((tetrahedron_volume(v[0], v[1], p, v[3]) << 8) + (vol >> 1)) / vol;
  n->mul[3] = ((tetrahedron_volume(v[0], v[1], v[2], p) << 8) + (vol >> 1)) / vol;
  uns j;
  for (j = 0; j < 4; j++)
    if (n->mul[j])
      break;
  for (uns i = 0; i < 4; i++)
    if (n->mul[i] == 0)
      n->ofs[i] = n->ofs[j];
}

static void
interpolation_table_init(void)
{
  DBG("Initializing color interpolation table");
  struct color_interpolation_node *n = color_interpolation_table =
    xmalloc(sizeof(struct color_interpolation_node) << (COLOR_CONV_OFS * 3));
  uns p[3];
  for (p[2] = 0; p[2] < (1 << COLOR_CONV_OFS); p[2]++)
    for (p[1] = 0; p[1] < (1 << COLOR_CONV_OFS); p[1]++)
      for (p[0] = 0; p[0] < (1 << COLOR_CONV_OFS); p[0]++)
        {
	  uns index;
          static const uns tetrahedra[5][4] = {
            {0000, 0001, 0010, 0100},
            {0110, 0111, 0100, 0010},
            {0101, 0100, 0111, 0001},
            {0011, 0010, 0001, 0111},
            {0111, 0001, 0010, 0100}};
	  if (p[0] + p[1] + p[2] <= (1 << COLOR_CONV_OFS))
	    index = 0;
	  else if ((1 << COLOR_CONV_OFS) + p[0] <= p[1] + p[2])
	    index = 1;
	  else if ((1 << COLOR_CONV_OFS) + p[1] <= p[0] + p[2])
	    index = 2;
	  else if ((1 << COLOR_CONV_OFS) + p[2] <= p[0] + p[1])
	    index = 3;
	  else
	    index = 4;
	  interpolate_tetrahedron(n, p, tetrahedra[index]);
	  n++;
	}
}

typedef void color_conv_func(double dest[3], double src[3]);

static void
conv_grid_init(struct color_grid_node **grid, color_conv_func func)
{
  if (*grid)
    return;
  struct color_grid_node *g = *grid = xmalloc((sizeof(struct color_grid_node)) << (COLOR_CONV_SIZE * 3));
  double src[3], dest[3];
  for (uns k = 0; k < (1 << COLOR_CONV_SIZE); k++)
    {
      src[2] = k * (255 / (double)((1 << COLOR_CONV_SIZE) - 1));
      for (uns j = 0; j < (1 << COLOR_CONV_SIZE); j++)
        {
          src[1] = j * (255/ (double)((1 << COLOR_CONV_SIZE) - 1));
          for (uns i = 0; i < (1 << COLOR_CONV_SIZE); i++)
            {
              src[0] = i * (255 / (double)((1 << COLOR_CONV_SIZE) - 1));
	      func(dest, src);
	      g->val[0] = CLAMP(dest[0] + 0.5, 0, 255);
	      g->val[1] = CLAMP(dest[1] + 0.5, 0, 255);
	      g->val[2] = CLAMP(dest[2] + 0.5, 0, 255);
	      g++;
	    }
	}
    }
}

static void
srgb_to_luv_func(double dest[3], double src[3])
{
  double srgb[3], xyz[3], luv[3];
  srgb[0] = src[0] / 255.;
  srgb[1] = src[1] / 255.;
  srgb[2] = src[2] / 255.;
  srgb_to_xyz_exact(xyz, srgb);
  xyz_to_luv_exact(luv, xyz);
  dest[0] = luv[0] * 2.55;
  dest[1] = luv[1] * (2.55 / 4) + 128;
  dest[2] = luv[2] * (2.55 / 4) + 128;
}

void
color_conv_init(void)
{
  interpolation_table_init();
  conv_grid_init(&srgb_to_luv_grid, srgb_to_luv_func);
}

void
color_conv_pixels(byte *dest, byte *src, uns count, struct color_grid_node *grid)
{
  while (count--)
    {
      color_conv_pixel(dest, src, grid);
      dest += 3;
      src += 3;
    }
}


/**************************** TESTS *******************************/

#ifdef TEST
#include <string.h>

static double
conv_error(u32 color, struct color_grid_node *grid, color_conv_func func)
{
  byte src[3], dest[3];
  src[0] = color & 255;
  src[1] = (color >> 8) & 255;
  src[2] = (color >> 16) & 255;
  color_conv_pixel(dest, src, grid);
  double src2[3], dest2[3];
  for (uns i = 0; i < 3; i++)
    src2[i] = src[i];
  func(dest2, src2);
  double err = 0;
  for (uns i = 0; i < 3; i++)
    err += (dest[i] - dest2[i]) * (dest[i] - dest2[i]);
  return err;
}

typedef void test_fn(byte *dest, byte *src);

static double
func_error(u32 color, test_fn test, color_conv_func func)
{
  byte src[3], dest[3];
  src[0] = color & 255;
  src[1] = (color >> 8) & 255;
  src[2] = (color >> 16) & 255;
  test(dest, src);
  double src2[3], dest2[3];
  for (uns i = 0; i < 3; i++)
    src2[i] = src[i];
  func(dest2, src2);
  double err = 0;
  for (uns i = 0; i < 3; i++)
    err += (dest[i] - dest2[i]) * (dest[i] - dest2[i]);
  return err;
}

static void
test_grid(byte *name, struct color_grid_node *grid, color_conv_func func)
{
  double max_err = 0, sum_err = 0;
  uns count = 100000;
  for (uns i = 0; i < count; i++)
    {
      double err = conv_error(random_max(0x1000000), grid, func);
      max_err = MAX(err, max_err);
      sum_err += err;
    }
  DBG("%s: error max=%f avg=%f", name, max_err, sum_err / count);
  if (max_err > 12)
    die("Too large error in %s conversion", name);
}

static void
test_func(byte *name, test_fn test, color_conv_func func)
{
  double max_err = 0, sum_err = 0;
  uns count = 100000;
  for (uns i = 0; i < count; i++)
    {
      double err = func_error(random_max(0x1000000), test, func);
      max_err = MAX(err, max_err);
      sum_err += err;
    }
  DBG("%s: error max=%f avg=%f", name, max_err, sum_err / count);
  if (max_err > 12)
    die("Too large error in %s conversion", name);
}

int
main(void)
{
  srgb_to_luv_init();
  test_func("func sRGB -> Luv", srgb_to_luv_pixel, srgb_to_luv_func);
  color_conv_init();
  test_grid("grid sRGB -> Luv", srgb_to_luv_grid, srgb_to_luv_func);
#ifdef LOCAL_DEBUG
#define CNT 1000000
#define TESTS 10
  byte *a = xmalloc(3 * CNT), *b = xmalloc(3 * CNT);
  for (uns i = 0; i < 3 * CNT; i++)
    a[i] = random_max(256);
  timestamp_t timer;
  init_timer(&timer);
  for (uns i = 0; i < TESTS; i++)
    memcpy(b, a, CNT * 3);
  DBG("memcpy time=%d", get_timer(&timer));
  init_timer(&timer);
  for (uns i = 0; i < TESTS; i++)
    srgb_to_luv_pixels(b, a, CNT);
  DBG("direct time=%d", get_timer(&timer));
  init_timer(&timer);
  for (uns i = 0; i < TESTS; i++)
    color_conv_pixels(b, a, CNT, srgb_to_luv_grid);
  DBG("grid time=%d", get_timer(&timer));
#endif
  return 0;
}
#endif

