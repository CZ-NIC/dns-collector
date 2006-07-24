/*
 *	Image Library -- libpng
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include <png.h>
#include <setjmp.h>

struct libpng_internals {
  png_structp png_ptr;
  png_infop info_ptr;
  png_infop end_ptr;
  png_uint_32 cols;
  png_uint_32 rows;
  int bit_depth;
  int color_type;
};

static png_voidp
libpng_malloc(png_structp png_ptr, png_size_t size)
{
  DBG("libpng_malloc(size=%u)", (uns)size);
  return mp_alloc(png_get_mem_ptr(png_ptr), size);
}

static void
libpng_free(png_structp png_ptr UNUSED, png_voidp ptr UNUSED)
{
  DBG("libpng_free()");
}

static void NONRET
libpng_error(png_structp png_ptr, png_const_charp msg)
{
  DBG("libpng_error()");
  image_thread_err(png_get_error_ptr(png_ptr), IMAGE_ERR_READ_FAILED, (byte *)msg);
  longjmp(png_jmpbuf(png_ptr), 1);
}

static void
libpng_warning(png_structp png_ptr UNUSED, png_const_charp msg UNUSED)
{
  DBG("libpng_warning(): %s", (byte *)msg);
}

static void
libpng_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
  DBG("libpng_read(): len=%d", (uns)length);
  if (unlikely(bread(png_get_io_ptr(png_ptr), data, length) < length))
    png_error(png_ptr, "Incomplete data");
}

static void
libpng_read_cancel(struct image_io *io)
{
  DBG("libpng_read_cancel()");
  struct libpng_internals *i = io->read_data;
  png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);
}

int
libpng_read_header(struct image_io *io)
{
  DBG("libpng_read_header()");
  struct libpng_internals *i = io->read_data = mp_alloc(io->internal_pool, sizeof(*i));
  i->png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
      io->thread, libpng_error, libpng_warning,
      io->internal_pool, libpng_malloc, libpng_free);
  if (unlikely(!i->png_ptr))
    goto err_create;
  i->info_ptr = png_create_info_struct(i->png_ptr);
  if (unlikely(!i->info_ptr))
    {
      png_destroy_read_struct(&i->png_ptr, NULL, NULL);
      goto err_create;
    }
  i->end_ptr = png_create_info_struct(i->png_ptr);
  if (unlikely(!i->end_ptr))
    {
      png_destroy_read_struct(&i->png_ptr, &i->info_ptr, NULL);
      goto err_create;
    }
  if (setjmp(png_jmpbuf(i->png_ptr)))
    {
      DBG("Libpng failed to read the image, longjump saved us");
      png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);
      return 0;
    }
  png_set_read_fn(i->png_ptr, io->fastbuf, libpng_read);
  png_set_user_limits(i->png_ptr, 0xffff, 0xffff);

  DBG("Reading image info");
  png_read_info(i->png_ptr, i->info_ptr);
  png_get_IHDR(i->png_ptr, i->info_ptr, &i->cols, &i->rows, &i->bit_depth, &i->color_type, NULL, NULL, NULL);

  if (!io->cols)
    io->cols = i->cols;
  if (!io->rows)
    io->rows = i->rows;
  if (!(io->flags & IMAGE_CHANNELS_FORMAT))
    switch (i->color_type)
      {
	case PNG_COLOR_TYPE_GRAY:
	  io->flags |= COLOR_SPACE_GRAYSCALE;
	  break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
	  io->flags |= COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA;
	  break;
	case PNG_COLOR_TYPE_RGB:
	  io->flags |= COLOR_SPACE_RGB;
	  break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
	case PNG_COLOR_TYPE_PALETTE:
	  io->flags |= COLOR_SPACE_RGB | IMAGE_ALPHA;
	  break;
	default:
	  png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);
	  image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Unknown color type");
	  break;
      }

  io->read_cancel = libpng_read_cancel;
  return 1;

err_create:
  image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Cannot create libpng read structure.");
  return 0;
}

int
libpng_read_data(struct image_io *io)
{
  DBG("libpng_read_data()");

  struct libpng_internals *i = io->read_data;

  /* Test supported pixel formats */
  switch (io->flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
      case COLOR_SPACE_RGB:
	break;
      default:
        png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);
	image_thread_err(io->thread, IMAGE_ERR_INVALID_PIXEL_FORMAT, "Unsupported color space.");
        return 0;
    }

  volatile int need_scale = io->cols != i->cols || io->rows != i->rows;
  struct image * volatile img = need_scale ? 
    image_new(io->thread, i->cols, i->rows, io->flags & IMAGE_PIXEL_FORMAT, NULL) :
    image_new(io->thread, i->cols, i->rows, io->flags, io->pool);
  if (!img)
    {
      png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);
      return 0;
    }

  if (setjmp(png_jmpbuf(i->png_ptr)))
    {
      DBG("Libpng failed to read the image, longjump saved us");
      png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);
      if (need_scale || !io->pool)
	image_destroy(io->thread, img);
      return 0;
    }

  /* Apply transformations */
  if (i->bit_depth == 16)
    png_set_strip_16(i->png_ptr);
  switch (i->color_type)
    {
      case PNG_COLOR_TYPE_PALETTE:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_GRAYSCALE)
	  {
	    png_set_palette_to_rgb(i->png_ptr);
	    png_set_rgb_to_gray_fixed(i->png_ptr, 1, 21267, 71514);
	  }
	else
	  png_set_palette_to_rgb(i->png_ptr);
	if ((io->flags & IMAGE_ALPHA) || (io->flags & IMAGE_PIXEL_FORMAT) == (COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED))
          png_set_add_alpha(i->png_ptr, 255, PNG_FILLER_AFTER);
	else
	  png_set_strip_alpha(i->png_ptr);
	break;
      case PNG_COLOR_TYPE_GRAY:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_RGB)
          png_set_gray_to_rgb(i->png_ptr);
	if (io->flags & IMAGE_ALPHA)
	  png_set_add_alpha(i->png_ptr, 255, PNG_FILLER_AFTER);
	break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_RGB)
          png_set_gray_to_rgb(i->png_ptr);
	if (!(io->flags & IMAGE_ALPHA))
          png_set_strip_alpha(i->png_ptr);
	break;
      case PNG_COLOR_TYPE_RGB:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_GRAYSCALE)
	  png_set_rgb_to_gray_fixed(i->png_ptr, 1, 21267, 71514);
	if ((io->flags & IMAGE_ALPHA) || (io->flags & IMAGE_PIXEL_FORMAT) == (COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED))
	  png_set_add_alpha(i->png_ptr, 255, PNG_FILLER_AFTER);
	break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_GRAYSCALE)
	  png_set_rgb_to_gray_fixed(i->png_ptr, 1, 21267, 71514);
	if (!(io->flags & IMAGE_ALPHA) && (io->flags & IMAGE_PIXEL_FORMAT) != (COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED))
          png_set_strip_alpha(i->png_ptr);
	break;
      default:
	ASSERT(0);
    }
  png_read_update_info(i->png_ptr, i->info_ptr);

  /* Read image data */
  DBG("Reading image data");
  byte *pixels = img->pixels;
  png_bytep rows[img->rows];
  for (uns r = 0; r < img->rows; r++, pixels += img->row_size)
    rows[r] = (png_bytep)pixels;
  png_read_image(i->png_ptr, rows);
  png_read_end(i->png_ptr, i->end_ptr);

  /* Destroy libpng read structure */
  png_destroy_read_struct(&i->png_ptr, &i->info_ptr, &i->end_ptr);

  /* Scale and store the resulting image */
  if (need_scale)
    {
      struct image *dest = image_new(io->thread, io->cols, io->rows, io->flags, io->pool);
      if (!dest)
        {
	  image_destroy(io->thread, img);
	  return 0;
	}
      if (!image_scale(io->thread, dest, img))
        {
	  image_destroy(io->thread, img);
	  if (!io->pool)
	    image_destroy(io->thread, dest);
	  return 0;
	}
      io->image = dest;
    }
  else
    io->image = img;
  io->image_destroy = !io->pool;
  
  return 1;
}

int
libpng_write(struct image_io *io)
{
  image_thread_err(io->thread, IMAGE_ERR_NOT_IMPLEMENTED, "Libpng writing not implemented.");
  return 0;
}
