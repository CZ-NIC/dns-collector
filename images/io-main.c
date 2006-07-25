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
  if (io->image_destroy)
    {
      image_destroy(io->thread, io->image);
      io->image_destroy = 0;
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
  mp_flush(pool);
  bzero(io, sizeof(*io));
  io->internal_pool = pool;
}

int
image_io_read_header(struct image_io *io)
{
  DBG("image_io_read_header()");
  image_io_read_cancel(io);
  image_io_image_destroy(io);
  switch (io->format) {
    case IMAGE_FORMAT_JPEG:
#if defined(CONFIG_LIBJPEG)
      return libjpeg_read_header(io);
#elif defined(CONFIG_LIBMAGICK)
      return libmagick_read_header(io);
#endif
      break;

    case IMAGE_FORMAT_PNG:
#if defined(CONFIG_LIBPNG)
      return libpng_read_header(io);
#elif defined(CONFIG_LIBMAGICK)
      return libmagick_read_header(io);
#endif
      break;

    case IMAGE_FORMAT_GIF:
#if defined(CONFIG_LIBUNGIG)
      return libungif_read_header(io);
#elif defined(CONFIG_LIBMAGICK)
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
#if defined(CONFIG_LIBJPEG)
      result = libjpeg_read_data(io);
#elif defined(CONFIG_LIBMAGICK)
      result = libmagick_read_data(io);
#else
      ASSERT(0);
#endif
      break;

    case IMAGE_FORMAT_PNG:
#if defined(CONFIG_LIBPNG)
      result = libpng_read_data(io);
#elif defined(CONFIG_LIBMAGICK)
      result = libmagick_read_data(io);
#else
      ASSERT(0);
#endif
      break;

    case IMAGE_FORMAT_GIF:
#if defined(CONFIG_LIBUNGIF)
      result = libungif_read_data(io);
#elif defined(CONFIG_LIBMAGICK)
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
      if (ref)
	io->image_destroy = 0;
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
#if defined(CONFIG_LIBJPEG)
      return libjpeg_write(io);
#elif defined(CONFIG_LIBMAGICK)
      return libmagick_write(io);
#endif
      break;

    case IMAGE_FORMAT_PNG:
#if defined(CONFIG_LIBMAGICK)
      return libmagick_write(io);
#endif
      break;

    case IMAGE_FORMAT_GIF:
#if defined(CONFIG_LIBMAGICK)
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
