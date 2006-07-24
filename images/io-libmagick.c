/*
 *	Image Library -- GrapgicsMagick
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "lib/lib.h"
#include "images/images.h"
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <magick/api.h>

int
libmagick_read_header(struct image_io *io)
{
  image_thread_err(io->thread, IMAGE_ERR_NOT_IMPLEMENTED, "GraphicsMagick read not implemented.");
  return 0;
}

int
libmagick_read_data(struct image_io *io UNUSED)
{
  ASSERT(0);
}

int
libmagick_write(struct image_io *io)
{
  image_thread_err(io->thread, IMAGE_ERR_NOT_IMPLEMENTED, "GraphicsMagick write not implemented.");
  return 0;
}

#if 0
struct magick_internals {
  ExceptionInfo exception;
  QuantizeInfo quantize;
  ImageInfo *info;
};

static inline void
magick_cleanup(struct image_io *io)
{
  DestroyImageInfo(io->internals->info);
  DestroyExceptionInfo(&io->internals->exception);
  DestroyMagick();
}

static int
magick_read_header(struct image_io *io)
{
  DBG("magick_read_header()");
  struct magick_internals *i = io->internals = mp_alloc(io->pool, sizeof(*i));

  InitializeMagick(NULL);
  GetExceptionInfo(&i->exception);
  i->info = CloneImageInfo(NULL);
  i->info->subrange = 1;
  GetQuantizeInfo(&i->quantize);
  i->quantize.colorspace = RGBColorspace;

  uns len = bfilesize(io->fastbuf);
  byte *buf = mp_alloc(io->pool, len);
  len = bread(io->fastbuf, buf, len);

  Image *image = BlobToImage(magick_info, imo->thumb_data, imo->thumb_size, &magick_exception);
  if (unlikely(!image))
    goto error;

  // FIXME
  return 1;
error:
  magick_cleanup(io);
  return 0;
}

static int
magick_read_data(struct image_io *io)
{
  DBG("magick_read_data()");

  // FIXME

  magick_cleanup(io);
  return 1;
}

static int
magick_decompress_thumbnail(struct image_obj *imo)
{
  DBG("Quantizing image");
  QuantizeImage(&magick_quantize, image);
  DBG("Converting pixels");
  PixelPacket *pixels = (PixelPacket *)AcquireImagePixels(image, 0, 0, image->columns, image->rows, &magick_exception);
  ASSERT(pixels);
  uns size = image->columns * image->rows;
  byte *p = imo->thumb.pixels = mp_alloc(imo->pool, imo->thumb.size = size * 3);
  for (uns i = 0; i < size; i++)
    {
      p[0] = pixels->red >> (QuantumDepth - 8);
      p[1] = pixels->green >> (QuantumDepth - 8);
      p[2] = pixels->blue >> (QuantumDepth - 8);
      p += 3;
      pixels++;
    }
  DestroyImage(image);
  return 1;
}
#endif
