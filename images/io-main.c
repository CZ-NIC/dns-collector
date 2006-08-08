/*
 *	Image Library -- Image compression/decompression interface
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "images/images.h"
#include "images/io-main.h"
#include <string.h>

void
image_io_init(struct image_thread *it, struct image_io *io)
{
  DBG("image_io_init()");
  bzero(io, sizeof(*io));
  io->thread = it;
  io->internal_pool = mp_new(1024);
}

static inline void
image_io_read_cancel(struct image_io *io)
{
  if (io->read_cancel)
    {
      io->read_cancel(io);
      io->read_cancel = NULL;
    }
}

static inline void
image_io_image_destroy(struct image_io *io)
{
  if (io->image && (io->flags & IMAGE_IO_NEED_DESTROY))
    {
      image_destroy(io->image);
      io->flags &= ~IMAGE_IO_NEED_DESTROY;
      io->image = NULL;
    }
}

void
image_io_cleanup(struct image_io *io)
{
  DBG("image_io_cleanup()");
  image_io_read_cancel(io);
  image_io_image_destroy(io);
  mp_delete(io->internal_pool);
}

void
image_io_reset(struct image_io *io)
{
  DBG("image_io_reset()");
  image_io_read_cancel(io);
  image_io_image_destroy(io);
  struct mempool *pool = io->internal_pool;
  struct image_thread *thread = io->thread;
  mp_flush(pool);
  bzero(io, sizeof(*io));
  io->internal_pool = pool;
  io->thread = thread;
}

int
image_io_read_header(struct image_io *io)
{
  DBG("image_io_read_header()");
  image_io_read_cancel(io);
  image_io_image_destroy(io);
  switch (io->format) {
    case IMAGE_FORMAT_JPEG:
#if defined(CONFIG_IMAGES_LIBJPEG)
      return libjpeg_read_header(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      return libmagick_read_header(io);
#endif
      break;

    case IMAGE_FORMAT_PNG:
#if defined(CONFIG_IMAGES_LIBPNG)
      return libpng_read_header(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      return libmagick_read_header(io);
#endif
      break;

    case IMAGE_FORMAT_GIF:
#if defined(CONFIG_IMAGES_LIBUNGIF) || defined(CONFIG_IMAGES_LIBGIF)
      return libungif_read_header(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      return libmagick_read_header(io);
#endif
      break;

    case IMAGE_FORMAT_UNDEFINED:
      // FIXME: auto-detect
      break;

    default:
      ASSERT(0);
  }
  image_thread_err(io->thread, IMAGE_ERR_INVALID_FILE_FORMAT, "Image format not supported.");
  return 0;
}

struct image *
image_io_read_data(struct image_io *io, int ref)
{
  DBG("image_io_read_data()");
  ASSERT(io->read_cancel);
  io->read_cancel = NULL;
  int result;
  switch (io->format) {
    case IMAGE_FORMAT_JPEG:
#if defined(CONFIG_IMAGES_LIBJPEG)
      result = libjpeg_read_data(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      result = libmagick_read_data(io);
#else
      ASSERT(0);
#endif
      break;

    case IMAGE_FORMAT_PNG:
#if defined(CONFIG_IMAGES_LIBPNG)
      result = libpng_read_data(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      result = libmagick_read_data(io);
#else
      ASSERT(0);
#endif
      break;

    case IMAGE_FORMAT_GIF:
#if defined(CONFIG_IMAGES_LIBUNGIF) || defined(CONFIG_IMAGES_LIBGIF)
      result = libungif_read_data(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      result = libmagick_read_data(io);
#else
      ASSERT(0);
#endif
      break;

    default:
      ASSERT(0);
  }
  if (result)
    {
      if (!ref)
	io->flags |= IMAGE_IO_NEED_DESTROY;
      else
	io->flags &= ~IMAGE_IO_NEED_DESTROY;
      return io->image;
    }
  else
    return NULL;
}

struct image *
image_io_read(struct image_io *io, int ref)
{
  if (!image_io_read_header(io))
    return NULL;
  return image_io_read_data(io, ref);
}

int
image_io_write(struct image_io *io)
{
  DBG("image_io_write()");
  image_io_read_cancel(io);
  switch (io->format) {
    case IMAGE_FORMAT_JPEG:
#if defined(CONFIG_IMAGES_LIBJPEG)
      return libjpeg_write(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      return libmagick_write(io);
#endif
      break;

    case IMAGE_FORMAT_PNG:
#if defined(CONFIG_IMAGES_LIBPNG)
      return libpng_write(io);
#elif defined(CONFIG_IMAGES_LIBMAGICK)
      return libmagick_write(io);
#endif
      break;

    case IMAGE_FORMAT_GIF:
#if defined(CONFIG_IMAGES_LIBMAGICK)
      return libmagick_write(io);
#endif
      break;

    default:
      break;
  }
  image_thread_err(io->thread, IMAGE_ERR_INVALID_FILE_FORMAT, "Image format not supported.");
  return 0;
}

byte *
image_format_to_extension(enum image_format format)
{
  switch (format)
    {
      case IMAGE_FORMAT_JPEG:
	return "jpg";
      case IMAGE_FORMAT_PNG:
	return "png";
      case IMAGE_FORMAT_GIF:
	return "gif";
      default:
	return NULL;
    }
}

enum image_format
image_extension_to_format(byte *extension)
{
  if (!strcasecmp(extension, "jpg"))
    return IMAGE_FORMAT_JPEG;
  if (!strcasecmp(extension, "jpeg"))
    return IMAGE_FORMAT_JPEG;
  if (!strcasecmp(extension, "png"))
    return IMAGE_FORMAT_PNG;
  if (!strcasecmp(extension, "gif"))
    return IMAGE_FORMAT_GIF;
  return IMAGE_FORMAT_UNDEFINED;
}

enum image_format
image_file_name_to_format(byte *file_name)
{
  byte *extension = strrchr(file_name, '.');
  return extension ? image_extension_to_format(extension + 1) : IMAGE_FORMAT_UNDEFINED;
}

struct image *
image_io_read_data_prepare(struct image_io_read_data_internals *rdi, struct image_io *io, uns cols, uns rows, uns flags)
{
  DBG("image_io_read_data_prepare()");
  if (rdi->need_transformations = io->cols != cols || io->rows != rows ||
      ((io->flags ^ flags) & IMAGE_NEW_FLAGS))
    return rdi->image = image_new(io->thread, cols, rows, flags & IMAGE_IO_IMAGE_FLAGS, NULL);
  else
    return rdi->image = image_new(io->thread, io->cols, io->rows, io->flags & IMAGE_IO_IMAGE_FLAGS, io->pool);
}

int
image_io_read_data_finish(struct image_io_read_data_internals *rdi, struct image_io *io)
{
  DBG("image_io_read_data_finish()");
  if (rdi->need_transformations)
    {
      /* Scale the image */
      if (io->cols != rdi->image->cols || io->rows != rdi->image->rows)
        {
	  DBG("Scaling image");
	  rdi->need_transformations = ((io->flags ^ rdi->image->flags) & IMAGE_NEW_FLAGS);
	  struct image *img = image_new(io->thread, io->cols, io->rows, rdi->image->flags, rdi->need_transformations ? NULL : io->pool);
	  if (unlikely(!img))
	    {
	      image_destroy(rdi->image);
	      return 0;
	    }
          if (unlikely(!image_scale(io->thread, img, rdi->image)))
            {
              image_destroy(rdi->image);
	      image_destroy(img);
	      return 0;
	    }
	  rdi->image = img;
	}

      /* Merge with background */
      if ((io->flags ^ rdi->image->flags) & IMAGE_ALPHA)
        {
	  DBG("Aplying background");
	  uns flags = rdi->image->flags & ~IMAGE_ALPHA;
	  if (!(rdi->need_transformations = (flags ^ io->flags) & (IMAGE_NEW_FLAGS & ~IMAGE_PIXELS_ALIGNED)))
	    flags = io->flags;
	  struct image *img = image_new(io->thread, io->cols, io->rows, flags, rdi->need_transformations ? NULL : io->pool);
	  if (unlikely(!img))
	    {
	      image_destroy(rdi->image);
	      return 0;
	    }
          if (unlikely(!image_apply_background(io->thread, img, rdi->image, &io->background_color)))
            {
              image_destroy(rdi->image);
	      image_destroy(img);
	      return 0;
	    }
	  rdi->image = img;
	}

      ASSERT(!rdi->need_transformations);
    }

  /* Success */
  io->image = rdi->image;
  return 1;
}

void
image_io_read_data_break(struct image_io_read_data_internals *rdi, struct image_io *io UNUSED)
{
  DBG("image_io_read_data_break()");
  if (rdi->image)
    image_destroy(rdi->image);
}
