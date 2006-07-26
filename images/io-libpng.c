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
#include "images/io-main.h"
#include <png.h>
#include <setjmp.h>

struct libpng_read_data {
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
libpng_read_error(png_structp png_ptr, png_const_charp msg)
{
  DBG("libpng_read_error()");
  image_thread_err_dup(png_get_error_ptr(png_ptr), IMAGE_ERR_READ_FAILED, (byte *)msg);
  longjmp(png_jmpbuf(png_ptr), 1);
}

static void NONRET
libpng_write_error(png_structp png_ptr, png_const_charp msg)
{
  DBG("libpng_write_error()");
  image_thread_err_dup(png_get_error_ptr(png_ptr), IMAGE_ERR_WRITE_FAILED, (byte *)msg);
  longjmp(png_jmpbuf(png_ptr), 1);
}

static void
libpng_warning(png_structp png_ptr UNUSED, png_const_charp msg UNUSED)
{
  DBG("libpng_warning(): %s", (byte *)msg);
}

static void
libpng_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
  DBG("libpng_read_fn(len=%u)", (uns)length);
  if (unlikely(bread((struct fastbuf *)png_get_io_ptr(png_ptr), (byte *)data, length) < length))
    png_error(png_ptr, "Incomplete data");
}

static void
libpng_write_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
  DBG("libpng_write_fn(len=%u)", (uns)length);
  bwrite((struct fastbuf *)png_get_io_ptr(png_ptr), (byte *)data, length);
}

static void
libpng_flush_fn(png_structp png_ptr UNUSED)
{
  DBG("libpng_flush_fn()");
}

static void
libpng_read_cancel(struct image_io *io)
{
  DBG("libpng_read_cancel()");

  struct libpng_read_data *rd = io->read_data;
  png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);
}

int
libpng_read_header(struct image_io *io)
{
  DBG("libpng_read_header()");

  /* Create libpng structures */
  struct libpng_read_data *rd = io->read_data = mp_alloc(io->internal_pool, sizeof(*rd));
  rd->png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
      io->thread, libpng_read_error, libpng_warning,
      io->internal_pool, libpng_malloc, libpng_free);
  if (unlikely(!rd->png_ptr))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Cannot create libpng read structure.");
      return 0;
    }
  rd->info_ptr = png_create_info_struct(rd->png_ptr);
  if (unlikely(!rd->info_ptr))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Cannot create libpng info structure.");
      png_destroy_read_struct(&rd->png_ptr, NULL, NULL);
      return 0;
    }
  rd->end_ptr = png_create_info_struct(rd->png_ptr);
  if (unlikely(!rd->end_ptr))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Cannot create libpng info structure.");
      png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, NULL);
      return 0;
    }

  /* Setup libpng longjump */
  if (unlikely(setjmp(png_jmpbuf(rd->png_ptr))))
    {
      DBG("Libpng failed to read the image, longjump saved us");
      png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);
      return 0;
    }

  /* Setup libpng IO */
  png_set_read_fn(rd->png_ptr, io->fastbuf, libpng_read_fn);
  png_set_user_limits(rd->png_ptr, IMAGE_MAX_SIZE, IMAGE_MAX_SIZE);

  /* Read header */
  png_read_info(rd->png_ptr, rd->info_ptr);
  png_get_IHDR(rd->png_ptr, rd->info_ptr, &rd->cols, &rd->rows, &rd->bit_depth, &rd->color_type, NULL, NULL, NULL);

  /* Fill image_io values */
  io->cols = rd->cols;
  io->rows = rd->rows;
  switch (rd->color_type)
    {
      case PNG_COLOR_TYPE_GRAY:
        io->flags = COLOR_SPACE_GRAYSCALE;
	io->number_of_colors = 1 << 8;
        break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
        io->flags = COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA;
	io->number_of_colors = 1 << 8;
        break;
      case PNG_COLOR_TYPE_RGB:
        io->flags = COLOR_SPACE_RGB;
	io->number_of_colors = 1 << 24;
        break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
	io->number_of_colors = 1 << 24;
        io->flags = COLOR_SPACE_RGB | IMAGE_ALPHA;
        break;
      case PNG_COLOR_TYPE_PALETTE:
        io->flags = COLOR_SPACE_RGB | IMAGE_ALPHA | IMAGE_IO_HAS_PALETTE;
	int num_palette;
	if (png_get_PLTE(rd->png_ptr, rd->info_ptr, NULL, &num_palette))
	  io->number_of_colors = num_palette;
	else
	  io->number_of_colors = 1 << rd->bit_depth;
        break;
      default:
        png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);
        image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Unknown color type");
        break;
    }

  /* Success */
  io->read_cancel = libpng_read_cancel;
  return 1;
}

int
libpng_read_data(struct image_io *io)
{
  DBG("libpng_read_data()");

  struct libpng_read_data *rd = io->read_data;

  /* Test supported pixel formats */
  switch (io->flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
      case COLOR_SPACE_RGB:
	break;
      default:
        png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);
	image_thread_err(io->thread, IMAGE_ERR_INVALID_PIXEL_FORMAT, "Unsupported color space.");
        return 0;
    }

  /* Prepare the image */
  struct image_io_read_data_internals rdi;
  if (unlikely(!image_io_read_data_prepare(&rdi, io, rd->cols, rd->rows)))
    {
      png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);
      return 0;
    }

  if (setjmp(png_jmpbuf(rd->png_ptr)))
    {
      DBG("Libpng failed to read the image, longjump saved us");
      png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);
      image_io_read_data_break(&rdi, io);
      return 0;
    }

  /* Apply transformations */
  if (rd->bit_depth == 16)
    png_set_strip_16(rd->png_ptr);
  switch (rd->color_type)
    {
      case PNG_COLOR_TYPE_PALETTE:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_GRAYSCALE)
	  {
	    png_set_palette_to_rgb(rd->png_ptr);
	    png_set_rgb_to_gray_fixed(rd->png_ptr, 1, 21267, 71514);
	  }
	else
	  png_set_palette_to_rgb(rd->png_ptr);
	if ((io->flags & IMAGE_ALPHA) || (io->flags & IMAGE_PIXEL_FORMAT) == (COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED))
          png_set_add_alpha(rd->png_ptr, 255, PNG_FILLER_AFTER);
	else
	  png_set_strip_alpha(rd->png_ptr);
	break;
      case PNG_COLOR_TYPE_GRAY:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_RGB)
          png_set_gray_to_rgb(rd->png_ptr);
	if (io->flags & IMAGE_ALPHA)
	  png_set_add_alpha(rd->png_ptr, 255, PNG_FILLER_AFTER);
	break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_RGB)
          png_set_gray_to_rgb(rd->png_ptr);
	if (!(io->flags & IMAGE_ALPHA))
          png_set_strip_alpha(rd->png_ptr);
	break;
      case PNG_COLOR_TYPE_RGB:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_GRAYSCALE)
	  png_set_rgb_to_gray_fixed(rd->png_ptr, 1, 21267, 71514);
	if ((io->flags & IMAGE_ALPHA) || (io->flags & IMAGE_PIXEL_FORMAT) == (COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED))
	  png_set_add_alpha(rd->png_ptr, 255, PNG_FILLER_AFTER);
	break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
	if ((io->flags & IMAGE_COLOR_SPACE) == COLOR_SPACE_GRAYSCALE)
	  png_set_rgb_to_gray_fixed(rd->png_ptr, 1, 21267, 71514);
	if (!(io->flags & IMAGE_ALPHA) && (io->flags & IMAGE_PIXEL_FORMAT) != (COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED))
          png_set_strip_alpha(rd->png_ptr);
	break;
      default:
	ASSERT(0);
    }
  png_read_update_info(rd->png_ptr, rd->info_ptr);

  /* Read image data */
  DBG("Reading image data");
  struct image *img = rdi.image;
  byte *pixels = img->pixels;
  png_bytep rows[img->rows];
  for (uns r = 0; r < img->rows; r++, pixels += img->row_size)
    rows[r] = (png_bytep)pixels;
  png_read_image(rd->png_ptr, rows);
  png_read_end(rd->png_ptr, rd->end_ptr);

  /* Destroy libpng read structure */
  png_destroy_read_struct(&rd->png_ptr, &rd->info_ptr, &rd->end_ptr);

  /* Finish the image  */
  return image_io_read_data_finish(&rdi, io);
}

int
libpng_write(struct image_io *io)
{
  DBG("libpng_write()");

  /* Create libpng structures */
  png_structp png_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING,
      io->thread, libpng_write_error, libpng_warning,
      io->internal_pool, libpng_malloc, libpng_free);
  if (unlikely(!png_ptr))
    {
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "Cannot create libpng write structure.");
      return 0;
    }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (unlikely(!info_ptr))
    {
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "Cannot create libpng info structure.");
      png_destroy_write_struct(&png_ptr, NULL);
      return 0;
    }

  /* Setup libpng longjump */
  if (unlikely(setjmp(png_jmpbuf(png_ptr))))
    {
      DBG("Libpng failed to write the image, longjump saved us.");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 0;
    }

  /* Setup libpng IO */
  png_set_write_fn(png_ptr, io->fastbuf, libpng_write_fn, libpng_flush_fn);

  /* Setup PNG parameters */
  struct image *img = io->image;
  switch (img->flags & IMAGE_PIXEL_FORMAT)
    {
      case COLOR_SPACE_GRAYSCALE | IMAGE_PIXELS_ALIGNED:
        png_set_IHDR(png_ptr, info_ptr, img->cols, img->rows, 8, PNG_COLOR_TYPE_GRAY,
	  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	break;
      case COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA | IMAGE_PIXELS_ALIGNED:
        png_set_IHDR(png_ptr, info_ptr, img->cols, img->rows, 8, PNG_COLOR_TYPE_GRAY_ALPHA,
	  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	break;
      case COLOR_SPACE_RGB:
        png_set_IHDR(png_ptr, info_ptr, img->cols, img->rows, 8, PNG_COLOR_TYPE_RGB,
	  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	break;
      case COLOR_SPACE_RGB | IMAGE_ALPHA | IMAGE_PIXELS_ALIGNED:
        png_set_IHDR(png_ptr, info_ptr, img->cols, img->rows, 8, PNG_COLOR_TYPE_RGB_ALPHA,
	  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	break;
      case COLOR_SPACE_RGB | IMAGE_PIXELS_ALIGNED:
        png_set_IHDR(png_ptr, info_ptr, img->cols, img->rows, 8, PNG_COLOR_TYPE_RGB,
	  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
	break;
      default:
        ASSERT(0);
    }
  png_write_info(png_ptr, info_ptr);

  /* Write pixels */
  byte *pixels = img->pixels;
  png_bytep rows[img->rows];
  for (uns r = 0; r < img->rows; r++, pixels += img->row_size)
    rows[r] = (png_bytep)pixels;
  png_write_image(png_ptr, rows);
  png_write_end(png_ptr, info_ptr);

  /* Free libpng structure */
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return 1;
}
