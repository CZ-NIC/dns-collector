/*
 *	Image Library -- Basic image manipulation
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "images/images.h"
#include <string.h>

#define MAX_IMAGE_SIZE (1 << 30)

void
image_thread_init(struct image_thread *it)
{
  DBG("image_thread_init()");
  bzero(it, sizeof(*it));
  it->pool = mp_new(1024);
}

void
image_thread_cleanup(struct image_thread *it)
{
  DBG("image_thread_cleanup()");
  mp_delete(it->pool);
}

void
image_thread_err_format(struct image_thread *it, uns code, char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  it->err_code = code;
  it->err_msg = mp_vprintf(it->pool, msg, args);
  va_end(args);
}

struct image *
image_new(struct image_thread *it, uns cols, uns rows, uns flags, struct mempool *pool)
{
  DBG("image_new(cols=%u rows=%u flags=0x%x pool=%p)", cols, rows, flags, pool);
  if (cols >= 0x10000 || rows >= 0x10000)
    {
      image_thread_err(it, IMAGE_ERR_INVALID_DIMENSIONS, "Image dimension(s) too large");
      return NULL;
    }
  struct image *img;
  uns pixel_size, row_size, image_size, align;
  switch (flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
	pixel_size = 1;
	break;
      case COLOR_SPACE_RGB:
	pixel_size = 3;
	break;
      default:
	ASSERT(0);
    }
  if (flags & IMAGE_ALPHA)
    pixel_size++;
  switch (pixel_size)
    {
      case 1:
      case 2:
      case 4:
	flags |= IMAGE_PIXELS_ALIGNED;
	break;
      case 3:
	if (flags & IMAGE_PIXELS_ALIGNED)
	  pixel_size = 4;
	break;
      default:
	ASSERT(0);
    }
  if (flags & IMAGE_SSE_ALIGNED)
    align = MAX(16, sizeof(uns));
  else if (flags & IMAGE_PIXELS_ALIGNED)
    align = pixel_size;
  else
    align = 1;
  row_size = cols * pixel_size;
  row_size = ALIGN(row_size, align);
  u64 image_size_64 = row_size * rows;
  if (image_size_64 > MAX_IMAGE_SIZE)
    {
      image_thread_err(it, IMAGE_ERR_INVALID_DIMENSIONS, "Image does not fit in memory");
      return NULL;
    }
  if (!(image_size = image_size_64))
    {
      image_thread_err(it, IMAGE_ERR_INVALID_DIMENSIONS, "Zero dimension(s)");
      return NULL;
    }
  uns size = sizeof(struct image) + image_size + MAX(16, sizeof(uns)) - 1 + sizeof(uns);
  img = pool ? mp_alloc(pool, size) : xmalloc(size);
  bzero(img, sizeof(struct image));
  byte *p = (byte *)img + sizeof(struct image);
  img->pixels = ALIGN_PTR(p, MAX(16, sizeof(uns)));
  img->flags = flags;
  img->pixel_size = pixel_size;
  img->cols = cols;
  img->rows = rows;
  img->row_size = row_size;
  img->image_size = image_size;
  DBG("img=%p flags=0x%x pixel_size=%u row_size=%u image_size=%u pixels=%p",
    img, img->flags, img->pixel_size, img->row_size, img->image_size, img->pixels);
  return img;
}

struct image *
image_clone(struct image_thread *it, struct image *src, uns flags, struct mempool *pool)
{
  DBG("image_clone(src=%p flags=0x%x pool=%p)", src, src->flags, pool);
  struct image *img;
  flags &= ~IMAGE_CHANNELS_FORMAT;
  flags |= src->flags & IMAGE_CHANNELS_FORMAT;
  if (!(img = image_new(it, src->cols, src->rows, flags, pool)))
    return NULL;
  if (img->image_size)
    {
      if (src->pixel_size != img->pixel_size)
        {
	  struct image *sec_img = src;
#         define IMAGE_WALK_INLINE
#         define IMAGE_WALK_DOUBLE
#         define IMAGE_WALK_DO_STEP do{ *(u32 *)pos = *(u32 *)sec_pos; }while(0)
#         include "images/image-walk.h"
	}
      else if (src->row_size != img->row_size)
        {
          byte *s = src->pixels;
          byte *d = img->pixels;
          uns bytes = src->cols * img->pixel_size;
	  for (uns row = src->rows; row--; )
            {
	      memcpy(d, s, bytes);
	      d += img->row_size;
	      s += src->row_size;
	    }
        }
      else
        memcpy(img->pixels, src->pixels, img->image_size);
    }
  return img;
}

void
image_destroy(struct image_thread *it UNUSED, struct image *img)
{
  DBG("image_destroy(img=%p)", img);
  xfree((byte *)img);
}

void
image_clear(struct image_thread *it UNUSED, struct image *img)
{
  DBG("image_clear(img=%p)", img);
  if (img->image_size)
    bzero(img->pixels, img->image_size);
}

byte *
color_space_to_name(enum color_space cs)
{
  return image_channels_format_to_name(cs);
}

byte *
image_channels_format_to_name(uns format)
{
  switch (format)
    {
      case COLOR_SPACE_GRAYSCALE:
	return "Gray";
      case COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA:
	return "GrayAlpha";
      case COLOR_SPACE_RGB:
	return "RGB";
      case COLOR_SPACE_RGB | IMAGE_ALPHA:
	return "RGBAlpha";
      default:
	return NULL;
    }
}

uns
image_name_to_channels_format(byte *name)
{
  if (!strcasecmp(name, "gray"))
    return COLOR_SPACE_GRAYSCALE;
  if (!strcasecmp(name, "grayscale"))
    return COLOR_SPACE_GRAYSCALE;
  if (!strcasecmp(name, "grayalpha"))
    return COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA;
  if (!strcasecmp(name, "grayscalealpha"))
    return COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA;
  if (!strcasecmp(name, "rgb"))
    return COLOR_SPACE_RGB;
  if (!strcasecmp(name, "rgbalpha"))
    return COLOR_SPACE_RGB + IMAGE_ALPHA;
  if (!strcasecmp(name, "rgba"))
    return COLOR_SPACE_RGB + IMAGE_ALPHA;
  return 0;
}
