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
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <magick/api.h>

#define MAX_FILE_SIZE (1 << 30)
#define QUANTUM_SCALE (QuantumDepth - 8)
#define QUANTUM_TO_BYTE(x) ((uns)(x) >> QUANTUM_SCALE)
#define BYTE_TO_QUANTUM(x) ((uns)(x) << QUANTUM_SCALE)
#define OPACITY_MAX ((1 << QuantumDepth) - 1)

struct magick_read_data {
  ExceptionInfo exception;
  ImageInfo *info;
  Image *image;
};

static inline void
libmagick_destroy_read_data(struct magick_read_data *rd)
{
  if (rd->image)
    DestroyImage(rd->image);
  DestroyImageInfo(rd->info);
  DestroyExceptionInfo(&rd->exception);
  DestroyMagick();
}

static void
libmagick_read_cancel(struct image_io *io)
{
  DBG("libmagick_read_cancel()");

  struct magick_read_data *rd = io->read_data;

  DestroyImage(rd->image);
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
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Too long stream.");
      return 0;
    }
  uns buf_size = file_size;
  byte *buf = xmalloc(buf_size);
  bread(io->fastbuf, buf, buf_size);

  /* Allocate read structure */
  struct magick_read_data *rd = io->read_data = mp_alloc(io->internal_pool, sizeof(*rd));

  /* Initialize GraphicsMagick */
  InitializeMagick(NULL);
  GetExceptionInfo(&rd->exception);
  rd->info = CloneImageInfo(NULL);
  rd->info->subrange = 1;

  /* Read the image */
  rd->image = BlobToImage(rd->info, buf, buf_size, &rd->exception);
  xfree(buf);
  if (unlikely(!rd->image))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "GraphicsMagick failed to read the image.");
      goto err;
    }
  if (unlikely(rd->image->columns > IMAGE_MAX_SIZE || rd->image->rows > IMAGE_MAX_SIZE))
    {
      image_thread_err(io->thread, IMAGE_ERR_INVALID_DIMENSIONS, "Image too large.");
      goto err;
    }

  /* Fill image parameters */
  io->cols = rd->image->columns;
  io->rows = rd->image->rows;
  switch (rd->image->colorspace)
    {
      case GRAYColorspace:
        io->flags |= COLOR_SPACE_GRAYSCALE | IMAGE_ALPHA;
        break;
      default:
        io->flags |= COLOR_SPACE_RGB | IMAGE_ALPHA;
        break;
    }
  io->number_of_colors = rd->image->colors;
  if (rd->image->storage_class == PseudoClass && rd->image->compression != JPEGCompression)
    io->has_palette = 1;

  io->read_cancel = libmagick_read_cancel;
  return 1;

err:
  libmagick_destroy_read_data(rd);
  return 0;
}

static inline byte
libmagick_pixel_to_gray(PixelPacket *pixel)
{
  return ((uns)pixel->red * 19660 + (uns)pixel->green * 38666 + (uns)pixel->blue * 7210) >> (16 + QUANTUM_SCALE);
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

  /* Allocate image for conversion */
  int need_scale = io->cols != rd->image->columns || io->rows != rd->image->rows;
  int need_destroy = need_scale || !io->pool;
  struct image *img = need_scale ?
    image_new(io->thread, rd->image->columns, rd->image->rows, io->flags & IMAGE_CHANNELS_FORMAT, NULL) :
    image_new(io->thread, io->cols, io->rows, io->flags, io->pool);
  if (unlikely(!img))
    goto err;

  /* Acquire pixels */
  PixelPacket *src = (PixelPacket *)AcquireImagePixels(rd->image, 0, 0, rd->image->columns, rd->image->rows, &rd->exception);
  if (unlikely(!src))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Cannot acquire image pixels.");
      goto err;
    }

  /* Convert pixels */
  switch (img->pixel_size)
    {
      case 1:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 1
#       define IMAGE_WALK_DO_STEP do{ \
	  pos[0] = libmagick_pixel_to_gray(src); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      case 2:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 2
#       define IMAGE_WALK_DO_STEP do{ \
	  pos[0] = libmagick_pixel_to_gray(src); \
	  pos[1] = QUANTUM_TO_BYTE(src->opacity); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      case 3:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 3
#       define IMAGE_WALK_DO_STEP do{ \
	  pos[0] = QUANTUM_TO_BYTE(src->red); \
	  pos[1] = QUANTUM_TO_BYTE(src->green); \
	  pos[2] = QUANTUM_TO_BYTE(src->blue); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      case 4:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 4
#       define IMAGE_WALK_DO_STEP do{ \
	  pos[0] = QUANTUM_TO_BYTE(src->red); \
	  pos[1] = QUANTUM_TO_BYTE(src->green); \
	  pos[2] = QUANTUM_TO_BYTE(src->blue); \
	  pos[3] = QUANTUM_TO_BYTE(src->opacity); \
	  src++; }while(0)
#       include "images/image-walk.h"
	break;

      default:
	ASSERT(0);
    }

  /* Free GraphicsMagick structures */
  libmagick_destroy_read_data(rd);

  /* Scale image */
  if (need_scale)
    {
      struct image *img2 = image_new(io->thread, io->cols, io->rows, io->flags, io->pool);
      if (unlikely(!img2))
        goto err2;
      int result = image_scale(io->thread, img2, img);
      image_destroy(img);
      img = img2;
      need_destroy = !io->pool;
      if (unlikely(!result))
	goto err2;
    }

  /* Success */
  io->image = img;
  io->image_destroy = need_destroy;
  return 1;

  /* Free structures */
err:
  libmagick_destroy_read_data(rd);
err2:
  if (need_destroy)
    image_destroy(img);
  return 0;
}

int
libmagick_write(struct image_io *io)
{
  DBG("libmagick_write()");

  /* Initialize GraphicsMagick */
  int result = 0;
  ExceptionInfo exception;
  ImageInfo *info;
  InitializeMagick(NULL);
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
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "GraphicsMagick failed to allocate the image.");
      goto err;
    }
  image->columns = img->cols;
  image->rows = img->rows;

  /* Get pixels */
  PixelPacket *pixels = SetImagePixels(image, 0, 0, img->cols, img->rows), *dest = pixels;
  if (unlikely(!pixels))
    {
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "Cannot get GraphicsMagick pixels.");
      goto err2;
    }

  /* Convert pixels */
  switch (img->pixel_size)
    {
      case 1:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 1
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(pos[0]); \
	  dest->blue = BYTE_TO_QUANTUM(pos[0]); \
	  dest->opacity = OPACITY_MAX; \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      case 2:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 2
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(pos[0]); \
	  dest->blue = BYTE_TO_QUANTUM(pos[0]); \
	  dest->opacity = BYTE_TO_QUANTUM(pos[1]); \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      case 3:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 3
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(pos[1]); \
	  dest->blue = BYTE_TO_QUANTUM(pos[2]); \
	  dest->opacity = OPACITY_MAX; \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      case 4:
#       define IMAGE_WALK_INLINE
#       define IMAGE_WALK_UNROLL 4
#       define IMAGE_WALK_COL_STEP 4
#       define IMAGE_WALK_DO_STEP do{ \
	  dest->red = BYTE_TO_QUANTUM(pos[0]); \
	  dest->green = BYTE_TO_QUANTUM(pos[1]); \
	  dest->blue = BYTE_TO_QUANTUM(pos[2]); \
	  dest->opacity = BYTE_TO_QUANTUM(pos[3]); \
	  dest++; }while(0)
#       include "images/image-walk.h"
	break;

      default:
	ASSERT(0);
    }

  /* Store pixels */
  if (unlikely(!SyncImagePixels(image)))
    {
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "Cannot sync GraphicsMagick pixels.");
      goto err2;
    }

  /* Write image */
  size_t buf_len = 0;
  void *buf = ImageToBlob(info, image, &buf_len, &exception);
  if (unlikely(!buf))
    {
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "GraphicsMagick failed to compress the image.");
      goto err2;
    }
  if (unlikely(buf_len > MAX_FILE_SIZE))
    {
      image_thread_err(io->thread, IMAGE_ERR_WRITE_FAILED, "Image too large.");
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
  DestroyMagick();
  return result;
}
