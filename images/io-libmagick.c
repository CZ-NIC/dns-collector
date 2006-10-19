/*
 *	Image Library -- GraphicsMagick (slow fallback library)
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
#include "images/error.h"
#include "images/color.h"
#include "images/io-main.h"

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <magick/api.h>
#include <pthread.h>

#define MAX_FILE_SIZE (1 << 30)
#define QUANTUM_SCALE (QuantumDepth - 8)
#define QUANTUM_TO_BYTE(x) ((uns)(x) >> QUANTUM_SCALE)
#define BYTE_TO_QUANTUM(x) ((uns)(x) << QUANTUM_SCALE)
#define ALPHA_TO_BYTE(x) (255 - QUANTUM_TO_BYTE(x))
#define BYTE_TO_ALPHA(x) (BYTE_TO_QUANTUM(255 - (x)))

static pthread_mutex_t libmagick_mutex = PTHREAD_MUTEX_INITIALIZER;
static uns libmagick_counter;

struct magick_read_data {
  ExceptionInfo exception;
  ImageInfo *info;
  Image *image;
};

int
libmagick_init(struct image_io *io UNUSED)
{
  pthread_mutex_lock(&libmagick_mutex);
  if (!libmagick_counter++)
    InitializeMagick(NULL);
  pthread_mutex_unlock(&libmagick_mutex);
  return 1;
}

void
libmagick_cleanup(struct image_io *io UNUSED)
{
  pthread_mutex_lock(&libmagick_mutex);
  if (!--libmagick_counter)
    DestroyMagick();
  pthread_mutex_unlock(&libmagick_mutex);
}

static void
libmagick_destroy_read_data(struct magick_read_data *rd)
{
  if (rd->image)
    DestroyImage(rd->image);
  DestroyImageInfo(rd->info);
  DestroyExceptionInfo(&rd->exception);
}

static void
libmagick_read_cancel(struct image_io *io)
{
  DBG("libmagick_read_cancel()");

  struct magick_read_data *rd = io->read_data;
  libmagick_destroy_read_data(rd);
}

int
libmagick_read_header(struct image_io *io)
{
  DBG("libmagick_read_header()");

  /* Read entire stream */
  sh_off_t file_size = bfilesize(io->fastbuf) - btell(io->fastbuf);
  if (unlikely(file_size > MAX_FILE_SIZE))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Too long stream.");
      return 0;
    }
  uns buf_size = file_size;
  byte *buf = xmalloc(buf_size);
  breadb(io->fastbuf, buf, buf_size);

  /* Allocate read structure */
  struct magick_read_data *rd = io->read_data = mp_alloc_zero(io->internal_pool, sizeof(*rd));

  /* Initialize GraphicsMagick */
  GetExceptionInfo(&rd->exception);
  rd->info = CloneImageInfo(NULL);
  rd->info->subrange = 1;

  /* Read the image */
  rd->image = BlobToImage(rd->info, buf, buf_size, &rd->exception);
  xfree(buf);
  if (unlikely(!rd->image))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "GraphicsMagick failed to read the image.");
      goto err;
    }
  if (unlikely(rd->image->columns > image_max_dim || rd->image->rows > image_max_dim))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_INVALID_DIMENSIONS, "Image too large.");
      goto err;
    }

  /* Fill image parameters */
  io->cols = rd->image->columns;
  io->rows = rd->image->rows;
  switch (rd->image->colorspace)
    {
      case GRAYColorspace:
        io->flags = COLOR_SPACE_GRAYSCALE;
        break;
      default:
        io->flags = COLOR_SPACE_RGB;
        break;
    }
  if (rd->image->matte)
    io->flags |= IMAGE_ALPHA;
  io->number_of_colors = rd->image->colors;
  if (rd->image->storage_class == PseudoClass && rd->image->compression != JPEGCompression)
    io->flags |= IMAGE_IO_HAS_PALETTE;

  io->read_cancel = libmagick_read_cancel;
  return 1;

err:
  libmagick_destroy_read_data(rd);
  return 0;
}

static inline byte
libmagick_pixel_to_gray(PixelPacket *pixel)
{
  return rgb_to_gray_func(pixel->red, pixel->green, pixel->blue) >> QUANTUM_SCALE;
}

int
libmagick_read_data(struct image_io *io)
{
  DBG("libmagick_read_data()");

  struct magick_read_data *rd = io->read_data;

  /* Quantize image */
  switch (rd->image->colorspace)
    {
      case RGBColorspace:
      case GRAYColorspace:
        break;
      default: ;
        QuantizeInfo quantize;
        GetQuantizeInfo(&quantize);
        quantize.colorspace = RGBColorspace;
        QuantizeImage(&quantize, rd->image);
	break;
    }

  /* Prepare the image */
  struct image_io_read_data_internals rdi;
  uns read_flags = io->flags;
  if ((read_flags & IMAGE_IO_USE_BACKGROUND) && !(read_flags & IMAGE_ALPHA))
    read_flags = (read_flags | IMAGE_ALPHA) & IMAGE_CHANNELS_FORMAT;
  if (unlikely(!image_io_read_data_prepare(&rdi, io, rd->image->columns, rd->image->rows, read_flags)))
    {
      libmagick_destroy_read_data(rd);
      return 0;
    }

  /* Acquire pixels */
  PixelPacket *src = (PixelPacket *)AcquireImagePixels(rd->image, 0, 0, rd->image->columns, rd->image->rows, &rd->exception);
  if (unlikely(!src))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Cannot acquire image pixels.");
      libmagick_destroy_read_data(rd);
      image_io_read_data_break(&rdi, io);
      return 0;
    }

  /* Convert pixels */
  switch (rdi.image->pixel_size)
    {
      case 1:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE (rdi.image)
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 1
#       define IMAGE_WALK_DO_STEP do{ \
	  walk_pos[0] = libmagick_pixel_to_gray(src); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      case 2:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE (rdi.image)
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 2
#       define IMAGE_WALK_DO_STEP do{ \
	  walk_pos[0] = libmagick_pixel_to_gray(src); \
	  walk_pos[1] = ALPHA_TO_BYTE(src->opacity); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      case 3:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE (rdi.image)
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 3
#       define IMAGE_WALK_DO_STEP do{ \
	  walk_pos[0] = QUANTUM_TO_BYTE(src->red); \
	  walk_pos[1] = QUANTUM_TO_BYTE(src->green); \
	  walk_pos[2] = QUANTUM_TO_BYTE(src->blue); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      case 4:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE (rdi.image)
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 4
#       define IMAGE_WALK_DO_STEP do{ \
	  walk_pos[0] = QUANTUM_TO_BYTE(src->red); \
	  walk_pos[1] = QUANTUM_TO_BYTE(src->green); \
	  walk_pos[2] = QUANTUM_TO_BYTE(src->blue); \
	  walk_pos[3] = ALPHA_TO_BYTE(src->opacity); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      default:
	ASSERT(0);
    }

  /* Free GraphicsMagick structures */
  libmagick_destroy_read_data(rd);

  /* Finish the image */
  return image_io_read_data_finish(&rdi, io);
}

int
libmagick_write(struct image_io *io)
{
  DBG("libmagick_write()");

  /* Initialize GraphicsMagick */
  int result = 0;
  ExceptionInfo exception;
  ImageInfo *info;
  GetExceptionInfo(&exception);
  info = CloneImageInfo(NULL);

  /* Setup image parameters and allocate the image*/
  struct image *img = io->image;
  switch (img->flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
	info->colorspace = GRAYColorspace;
	break;
      case COLOR_SPACE_RGB:
        info->colorspace = RGBColorspace;
        break;
      default:
        ASSERT(0);
    }
  switch (io->format)
    {
      case IMAGE_FORMAT_JPEG:
	strcpy(info->magick, "JPEG");
	if (io->jpeg_quality)
	  info->quality = MIN(io->jpeg_quality, 100);
	break;
      case IMAGE_FORMAT_PNG:
	strcpy(info->magick, "PNG");
	break;
      case IMAGE_FORMAT_GIF:
        strcpy(info->magick, "GIF");
	break;
      default:
        ASSERT(0);
    }
  Image *image = AllocateImage(info);
  if (unlikely(!image))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_WRITE_FAILED, "GraphicsMagick failed to allocate the image.");
      goto err;
    }
  image->columns = img->cols;
  image->rows = img->rows;

  /* Get pixels */
  PixelPacket *pixels = SetImagePixels(image, 0, 0, img->cols, img->rows), *dest = pixels;
  if (unlikely(!pixels))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_WRITE_FAILED, "Cannot get GraphicsMagick pixels.");
      goto err2;
    }

  /* Convert pixels */
  switch (img->pixel_size)
    {
      case 1:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE img
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 1
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->blue = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->opacity = 0; \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      case 2:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE img
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 2
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->blue = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->opacity = BYTE_TO_ALPHA(walk_pos[1]); \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      case 3:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE img
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 3
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(walk_pos[1]); \
	  dest->blue = BYTE_TO_QUANTUM(walk_pos[2]); \
	  dest->opacity = 0; \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      case 4:
#	define IMAGE_WALK_PREFIX(x) walk_##x
#       define IMAGE_WALK_INLINE
#	define IMAGE_WALK_IMAGE img
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 4
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(walk_pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(walk_pos[1]); \
	  dest->blue = BYTE_TO_QUANTUM(walk_pos[2]); \
	  dest->opacity = BYTE_TO_ALPHA(walk_pos[3]); \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      default:
	ASSERT(0);
    }

  /* Store pixels */
  if (unlikely(!SyncImagePixels(image)))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_WRITE_FAILED, "Cannot sync GraphicsMagick pixels.");
      goto err2;
    }

  /* Write image */
  size_t buf_len = 0;
  void *buf = ImageToBlob(info, image, &buf_len, &exception);
  if (unlikely(!buf))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_WRITE_FAILED, "GraphicsMagick failed to compress the image.");
      goto err2;
    }
  if (unlikely(buf_len > MAX_FILE_SIZE))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_WRITE_FAILED, "Image too large.");
      goto err2;
    }

  /* Write to stream */
  bwrite(io->fastbuf, buf, buf_len);

  /* Success */
  result = 1;

err2:
  DestroyImage(image);
err:
  DestroyImageInfo(info);
  DestroyExceptionInfo(&exception);
  return result;
}
