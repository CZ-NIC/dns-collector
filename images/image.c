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
#include "images/error.h"
#include "images/color.h"

#include <string.h>

static inline uns
flags_to_pixel_size(uns flags)
{
  uns pixel_size = color_space_channels[flags & IMAGE_COLOR_SPACE];
  if (flags & IMAGE_ALPHA)
    pixel_size++;
  return pixel_size;
}

struct image *
image_new(struct image_context *ctx, uns cols, uns rows, uns flags, struct mempool *pool)
{
  DBG("image_new(cols=%u rows=%u flags=0x%x pool=%p)", cols, rows, flags, pool);
  flags &= IMAGE_NEW_FLAGS;
  if (unlikely(!image_dimensions_valid(cols, rows)))
    {
      IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_DIMENSIONS, "Invalid image dimensions (%ux%u)", cols, rows);
      return NULL;
    }
  struct image *img;
  uns channels, pixel_size, row_pixels_size, row_size, align;
  pixel_size = channels = flags_to_pixel_size(flags);
  if (!channels || channels > 4)
    {
      IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Invalid number of color channels (%u)", channels);
      return NULL;
    }
  switch (channels)
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
    align = IMAGE_SSE_ALIGN_SIZE;
  else if (flags & IMAGE_PIXELS_ALIGNED)
    align = pixel_size;
  else
    align = 1;
  row_pixels_size = cols * pixel_size;
  row_size = ALIGN_TO(row_pixels_size, align);
  u64 image_size_64 = (u64)row_size * rows;
  u64 bytes_64 = image_size_64 + (sizeof(struct image) + IMAGE_SSE_ALIGN_SIZE - 1 + sizeof(uns));
  if (unlikely(bytes_64 > image_max_bytes))
    {
      IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_DIMENSIONS, "Image does not fit in memory");
      return NULL;
    }
  if (pool)
    img = mp_alloc(pool, bytes_64);
  else
    {
      img = xmalloc(bytes_64);
      flags |= IMAGE_NEED_DESTROY;
    }
  bzero(img, sizeof(struct image));
  byte *p = (byte *)img + sizeof(struct image);
  img->pixels = ALIGN_PTR(p, IMAGE_SSE_ALIGN_SIZE);
  img->flags = flags;
  img->channels = channels;
  img->pixel_size = pixel_size;
  img->cols = cols;
  img->rows = rows;
  img->row_size = row_size;
  img->row_pixels_size = row_pixels_size;
  img->image_size = image_size_64;
  DBG("img=%p flags=0x%x pixel_size=%u row_size=%u image_size=%u pixels=%p",
    img, img->flags, img->pixel_size, img->row_size, img->image_size, img->pixels);
  return img;
}

struct image *
image_clone(struct image_context *ctx, struct image *src, uns flags, struct mempool *pool)
{
  DBG("image_clone(src=%p flags=0x%x pool=%p)", src, src->flags, pool);
  struct image *img;
  flags &= IMAGE_NEW_FLAGS & ~IMAGE_CHANNELS_FORMAT;
  flags |= src->flags & IMAGE_CHANNELS_FORMAT;
  if (!(img = image_new(ctx, src->cols, src->rows, flags, pool)))
    return NULL;
  ASSERT(src->channels == img->channels);
  if (img->image_size)
    {
      if (src->pixel_size != img->pixel_size) /* conversion between aligned and unaligned RGB */
        {
	  ASSERT(src->channels == 3);
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE img
#	  define IMAGE_WALK_SEC_IMAGE src
#         define IMAGE_WALK_DOUBLE
#         define IMAGE_WALK_DO_STEP do{ walk_pos[0] = walk_sec_pos[0]; walk_pos[1] = walk_sec_pos[1]; walk_pos[2] = walk_sec_pos[2]; }while(0)
#         include "images/image-walk.h"
	}
      else if (src->row_size != img->row_size || ((img->flags | src->flags) & IMAGE_GAPS_PROTECTED))
        {
          byte *s = src->pixels;
          byte *d = img->pixels;
	  for (uns row = src->rows; row--; )
            {
	      memcpy(d, s, src->row_pixels_size);
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
image_destroy(struct image *img)
{
  DBG("image_destroy(img=%p)", img);
  if (img->flags & IMAGE_NEED_DESTROY)
    xfree(img);
}

void
image_clear(struct image_context *ctx UNUSED, struct image *img)
{
  DBG("image_clear(img=%p)", img);
  if (img->image_size)
    if (img->flags & IMAGE_GAPS_PROTECTED)
      {
        byte *p = img->pixels;
        uns bytes = img->cols * img->pixel_size;
	for (uns row = img->rows; row--; p += img->row_size)
	  bzero(p, bytes);
      }
    else
      bzero(img->pixels, img->image_size);
}

struct image *
image_init_matrix(struct image_context *ctx, struct image *img, byte *pixels, uns cols, uns rows, uns row_size, uns flags)
{
  DBG("image_init_matrix(img=%p pixels=%p cols=%u rows=%u row_size=%u flags=0x%x)", img, pixels, cols, rows, row_size, flags);
  if (unlikely(!image_dimensions_valid(cols, rows)))
    {
      IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_DIMENSIONS, "Invalid image dimensions (%ux%u)", cols, rows);
      return NULL;
    }
  img->pixels = pixels;
  img->cols = cols;
  img->rows = rows;
  img->pixel_size = img->channels = flags_to_pixel_size(flags);
  img->row_size = row_size;
  img->row_pixels_size = cols * img->pixel_size;
  img->image_size = rows * row_size;
  img->flags = flags & (IMAGE_NEW_FLAGS | IMAGE_GAPS_PROTECTED);
  return img;
}

struct image *
image_init_subimage(struct image_context *ctx UNUSED, struct image *img, struct image *src, uns left, uns top, uns cols, uns rows)
{
  DBG("image_init_subimage(img=%p src=%p left=%u top=%u cols=%u rows=%u)", img, src, left, top, cols, rows);
  ASSERT(left + cols <= src->cols && top + rows <= src->rows);
  img->pixels = src->pixels + left * src->pixel_size + top * src->row_size;
  img->cols = cols;
  img->rows = rows;
  img->pixel_size = img->channels = src->pixel_size;
  img->row_size = src->row_size;
  img->row_pixels_size = cols * src->pixel_size;
  img->image_size = src->row_size * rows;
  img->flags = src->flags & IMAGE_NEW_FLAGS;
  img->flags |= IMAGE_GAPS_PROTECTED;
  return img;
}

byte *
image_channels_format_to_name(uns format, byte *buf)
{
  byte *cs_name = color_space_id_to_name(format & IMAGE_COLOR_SPACE);
  uns l = strlen(cs_name);
  memcpy(buf, cs_name, l + 1);
  if (format & IMAGE_ALPHA)
    strcpy(buf + l, "+Alpha");
  return buf;
}

uns
image_name_to_channels_format(byte *name)
{
  uns i;
  if (i = color_space_name_to_id(name))
    return i;
  uns l = strlen(name);
  if (l > 6 && !strcasecmp(name + l - 5, "+alpha"))
    {
      byte buf[l + 1];
      memcpy(buf, name, l - 6);
      buf[l - 6] = 0;
      if (i = color_space_name_to_id(name))
	return i;
    }
  return 0;
}
