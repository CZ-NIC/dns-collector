/*
 *	Image Library -- Image Cards Manipulations
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 *
 *	FIXME:
 *	- improve thumbnail creation in gatherer... faster compression,
 *	  only grayscale/RGB colorspaces and maybe fixed headers (abbreviated datastreams in libjpeg)
 *	- hook memory allocation managers, get rid of multiple initializations
 *	- supply background color to transparent PNG images
 *	- optimize decompression parameters
 *	- create interface for thumbnail compression (for gatherer) and reading (MUX)
 *	- benchmatk libraries
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/base224.h"
#include "lib/mempool.h"
#include "sherlock/object.h"
#include "images/images.h"
#include "images/image-obj.h"

#include <stdio.h>
#include <alloca.h>

/* Selection of libraries to use */
#define USE_LIBPNG
#define USE_LIBJPEG
#define USE_MAGICK

#if defined(USE_LIBPNG) && defined(USE_LIBJPEG)
#undef USE_MAGICK
#endif


/*********************************  LIBPNG Library ****************************************/

#ifdef USE_LIBPNG

#include <png.h>
#include <setjmp.h>

static struct mempool *libpng_pool;
static byte *libpng_buf;
static uns libpng_len;

static png_voidp
libpng_malloc(png_structp png_ptr UNUSED, png_size_t size)
{
  DBG("libpng_malloc(): size=%d", (uns)size);
  return mp_alloc(libpng_pool, size);
}

static void
libpng_free(png_structp png_ptr UNUSED, png_voidp ptr UNUSED)
{
  DBG("libpng_free()");
}

static void NONRET
libpng_error(png_structp png_ptr, png_const_charp msg UNUSED)
{
  DBG("libpng_error(): msg=%s", (byte *)msg);
  longjmp(png_jmpbuf(png_ptr), 1);
}

static void
libpng_warning(png_structp png_ptr UNUSED, png_const_charp msg UNUSED)
{
  DBG("libpng_warning(): msg=%s", (byte *)msg);
}

static void
libpng_read_data(png_structp png_ptr UNUSED, png_bytep data, png_size_t length)
{
  DBG("libpng_read_data(): len=%d", (uns)length);
  if (unlikely(libpng_len < length))
    png_error(png_ptr, "Incomplete data");
  memcpy(data, libpng_buf, length);
  libpng_buf += length;
  libpng_len -= length;
}

static inline void
libpng_decompress_thumbnails_init(void)
{
}

static inline void
libpng_decompress_thumbnails_done(void)
{
}

static int
libpng_decompress_thumbnail(struct image_obj *imo)
{
  /* create libpng read structure */
  DBG("Creating libpng read structure");
  libpng_pool = imo->pool;
  libpng_buf = imo->thumb_data;
  libpng_len = imo->thumb_size;
  png_structp png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
      NULL, libpng_error, libpng_warning,
      NULL, libpng_malloc, libpng_free);
  if (unlikely(!png_ptr))
    return 0;
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (unlikely(!info_ptr))
    {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return 0;
    }
  png_infop end_ptr = png_create_info_struct(png_ptr);
  if (unlikely(!end_ptr))
    {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 0;
    }
  if (setjmp(png_jmpbuf(png_ptr)))
    {
      DBG("Libpng failed to read the image, longjump saved us");
      png_destroy_read_struct(&png_ptr, &info_ptr, &end_ptr);
      return 0;
    }
  png_set_read_fn(png_ptr, NULL, libpng_read_data);

  /* Read image info */
  DBG("Reading image info");
  png_read_info(png_ptr, info_ptr);
  png_uint_32 width, height;
  int bit_depth, color_type;
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
  ASSERT(width == imo->thumb.width && height == imo->thumb.height);

  /* Apply transformations */
  imo->thumb.flags = 0;
  if (bit_depth == 16)
    png_set_strip_16(png_ptr);
  switch (color_type)
    {
      case PNG_COLOR_TYPE_PALETTE:
	png_set_palette_to_rgb(png_ptr);
        png_set_strip_alpha(png_ptr);
	break;
      case PNG_COLOR_TYPE_GRAY:
	imo->thumb.flags |= IMAGE_GRAYSCALE;
        png_set_gray_to_rgb(png_ptr);
	break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
	imo->thumb.flags |= IMAGE_GRAYSCALE;
        png_set_gray_to_rgb(png_ptr);
        png_set_strip_alpha(png_ptr);
	break;
      case PNG_COLOR_TYPE_RGB:
	break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
        png_set_strip_alpha(png_ptr);
	break;
      default:
	ASSERT(0);
    }
  png_read_update_info(png_ptr, info_ptr);
  ASSERT(png_get_channels(png_ptr, info_ptr) == 3);

  /* Read image data */
  DBG("Reading image data");
  byte *pixels = imo->thumb.pixels = mp_alloc(imo->pool, imo->thumb.size = width * height * 3);
  png_bytep rows[height];
  for (uns i = 0; i < height; i++, pixels += width * 3)
    rows[i] = (png_bytep)pixels;
  png_read_image(png_ptr, rows);
  png_read_end(png_ptr, end_ptr);
 
  /* Destroy libpng read structure */
  png_destroy_read_struct(&png_ptr, &info_ptr, &end_ptr);
  return 1;
}

#endif /* USE_LIBPNG */



/*******************************  LIBJPEG Library *************************************/

#ifdef USE_LIBJPEG

#include <jpeglib.h>
#include <setjmp.h>

struct libjpeg_err {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buf;
};

static void NONRET
libjpeg_error_exit(j_common_ptr cinfo)
{
  DBG("libjpeg_error_exit()");
  longjmp(((struct libjpeg_err *)(cinfo)->err)->setjmp_buf, 1);
}

static void
libjpeg_emit_message(j_common_ptr cinfo UNUSED, int msg_level UNUSED)
{
  DBG("libjpeg_emit_message(): level=%d", msg_level);
  /* if (unlikely(msg_level == -1))
    longjmp(((struct libjpeg_err *)(cinfo)->err)->setjmp_buf, 1); */
}

static void
libjpeg_init_source(j_decompress_ptr cinfo UNUSED)
{
  DBG("libjpeg_init_source()");
}

static boolean NONRET
libjpeg_fill_input_buffer(j_decompress_ptr cinfo)
{ 
  DBG("libjpeg_fill_input_buffer()");
  longjmp(((struct libjpeg_err *)(cinfo)->err)->setjmp_buf, 1);
}

static void
libjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes) 
{
  DBG("libjpeg_skip_input_data(): len=%d", (int)num_bytes);
  if (num_bytes > 0)
    {
      cinfo->src->next_input_byte += num_bytes;
      cinfo->src->bytes_in_buffer -= num_bytes;
    }
}

static inline void
libjpeg_decompress_thumbnails_init(void)
{
}

static inline void
libjpeg_decompress_thumbnails_done(void)
{
}

static int
libjpeg_decompress_thumbnail(struct image_obj *imo)
{
  /* Create libjpeg read structure */
  DBG("Creating libjpeg read structure");
  struct jpeg_decompress_struct cinfo;
  struct libjpeg_err err;
  cinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = libjpeg_error_exit;
  err.pub.emit_message = libjpeg_emit_message;
  if (setjmp(err.setjmp_buf))
    {
      DBG("Libjpeg failed to read the image, longjump saved us");
      jpeg_destroy_decompress(&cinfo);
      return 0;
    }
  jpeg_create_decompress(&cinfo);

  /* Initialize source manager */
  struct jpeg_source_mgr src;
  cinfo.src = &src;
  src.next_input_byte = imo->thumb_data;
  src.bytes_in_buffer = imo->thumb_size;
  src.init_source = libjpeg_init_source;
  src.fill_input_buffer = libjpeg_fill_input_buffer;
  src.skip_input_data = libjpeg_skip_input_data;
  src.resync_to_restart = jpeg_resync_to_restart;
  src.term_source = libjpeg_init_source;

  /* Read JPEG header and setup decompression options */
  DBG("Reading image header");
  jpeg_read_header(&cinfo, TRUE);
  imo->thumb.flags = 0;
  if (cinfo.out_color_space == JCS_GRAYSCALE)
    imo->thumb.flags |= IMAGE_GRAYSCALE;
  else
    cinfo.out_color_space = JCS_RGB;

  /* Decompress the image */
  DBG("Reading image data");
  jpeg_start_decompress(&cinfo);
  ASSERT(imo->thumb.width == cinfo.output_width && imo->thumb.height == cinfo.output_height);
  ASSERT(sizeof(JSAMPLE) == 1);
  byte *pixels = imo->thumb.pixels = mp_alloc(imo->pool, imo->thumb.size = cinfo.output_width * cinfo.output_height * 3);
  if (cinfo.out_color_space == JCS_RGB)
    { /* Read RGB pixels */
      uns size = cinfo.output_width * 3;
      while (cinfo.output_scanline < cinfo.output_height)
        {
          jpeg_read_scanlines(&cinfo, (JSAMPLE **)&pixels, 1);
	  pixels += size;
        }
    }
  else
    { /* Read grayscale pixels */
      JSAMPLE buf[cinfo.output_width], *buf_end = buf + cinfo.output_width;
      while (cinfo.output_scanline < cinfo.output_height)
        {
          JSAMPLE *p = buf;
          jpeg_read_scanlines(&cinfo, &p, 1);
          for (; p != buf_end; p++)
            {
              pixels[0] = pixels[1] = pixels[2] = p[0];
	      pixels += 3;
	    }
        }
    }
  jpeg_finish_decompress(&cinfo);

  /* Destroy libjpeg object and leave */
  jpeg_destroy_decompress(&cinfo);
  return 1;
}

#endif /* USE_LIBJPEG */



/******************************  GraphicsMagick Library ******************************/

#ifdef USE_MAGICK

#include <magick/api.h>

static ExceptionInfo magick_exception;
static QuantizeInfo magick_quantize;
static ImageInfo *magick_info;

static void
magick_decompress_thumbnails_init(void)
{
  DBG("Initializing magick thumbnails decompression");
  InitializeMagick(NULL);
  GetExceptionInfo(&magick_exception);
  magick_info = CloneImageInfo(NULL);
  magick_info->subrange = 1;
  GetQuantizeInfo(&magick_quantize);
  magick_quantize.colorspace = RGBColorspace;
}

static void
magick_decompress_thumbnails_done(void)
{
  DBG("Finalizing magick thumbnails decompression");
  DestroyImageInfo(magick_info);
  DestroyExceptionInfo(&magick_exception);
  DestroyMagick();
}

static int
magick_decompress_thumbnail(struct image_obj *imo)
{
  DBG("Reading image data");
  Image *image = BlobToImage(magick_info, imo->thumb_data, imo->thumb_size, &magick_exception);
  if (unlikely(!image))
    return 0;
  ASSERT(image->columns == imo->thumb.width && image->rows == imo->thumb.height);
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

#endif /* USE_MAGICK */



/*************************************************************************************/

static int
extract_image_info(struct image_obj *imo)
{
  DBG("Parsing image info attribute");
  ASSERT(!(imo->flags & IMAGE_OBJ_VALID_INFO));
  imo->flags |= IMAGE_OBJ_VALID_INFO;
  byte *info = obj_find_aval(imo->obj, 'G');
  if (!info)
    {
      DBG("Attribute G not found");
      return 0;
    }
  uns colors;
  byte color_space[MAX_ATTR_SIZE], thumb_format[MAX_ATTR_SIZE];
  UNUSED uns cnt = sscanf(info, "%d%d%s%d%d%d%s", &imo->width, &imo->height, color_space, &colors, &imo->thumb.width, &imo->thumb.height, thumb_format);
  ASSERT(cnt == 7);
  switch (*thumb_format)
    {
      case 'j':
	imo->thumb_format = IMAGE_OBJ_FORMAT_JPEG;
	break;
      case 'p':
	imo->thumb_format = IMAGE_OBJ_FORMAT_PNG;
	break;
      default:
	ASSERT(0);
    }
  return 1;
}

static int
extract_thumb_data(struct image_obj *imo)
{
  DBG("Extracting thumbnail data");
  ASSERT(!(imo->flags & IMAGE_OBJ_VALID_DATA) && 
      (imo->flags & IMAGE_OBJ_VALID_INFO));
  imo->flags |= IMAGE_OBJ_VALID_DATA;
  struct oattr *attr = obj_find_attr(imo->obj, 'N');
  if (!attr)
    {
      DBG("There is no thumbnail attribute N");
      return 0;
    }
  uns count = 0;
  for (struct oattr *a = attr; a; a = a->same)
    count++;
  byte b224[count * MAX_ATTR_SIZE], *b = b224;
  for (struct oattr *a = attr; a; a = a->same)
    for (byte *s = a->val; *s; )
      *b++ = *s++;
  ASSERT(b != b224);
  uns size = b - b224;
  imo->thumb_data = mp_alloc(imo->pool, size);
  imo->thumb_size = base224_decode(imo->thumb_data, b224, size);
  DBG("Thumbnail data size is %d", imo->thumb_size);
  return 1;
}

static int
extract_thumb_image(struct image_obj *imo)
{
  DBG("Decompressing thumbnail image");
  ASSERT(!(imo->flags & IMAGE_OBJ_VALID_IMAGE) &&
      (imo->flags & IMAGE_OBJ_VALID_INFO) &&
      (imo->flags & IMAGE_OBJ_VALID_DATA));
  imo->flags |= IMAGE_OBJ_VALID_IMAGE;
  switch (imo->thumb_format)
    {
      case IMAGE_OBJ_FORMAT_JPEG:
#if defined(USE_LIBJPEG)
        return libjpeg_decompress_thumbnail(imo);
#elif defined(USE_MAGICK)
        return magick_decompress_thumbnail(imo);
#else
        DBG("JPEG not supported");
        return 0;
#endif
      case IMAGE_OBJ_FORMAT_PNG:
#if defined(USE_LIBPNG)
        return libpng_decompress_thumbnail(imo);
#elif defined(USE_MAGICK)
        return magick_decompress_thumbnail(imo);
#else
        DBG("PNG not supported");
        return 0;
#endif      
      default:
	ASSERT(0);
    }
}

void
imo_decompress_thumbnails_init(void)
{
#ifdef USE_LIBPNG
  libpng_decompress_thumbnails_init();
#endif
#ifdef USE_LIBJPEG
  libjpeg_decompress_thumbnails_init();
#endif
#ifdef USE_MAGICK
  magick_decompress_thumbnails_init();
#endif
}

void
imo_decompress_thumbnails_done(void)
{
#ifdef USE_MAGICK
  magick_decompress_thumbnails_done();
#endif
#ifdef USE_LIBJPEG
  libjpeg_decompress_thumbnails_done();
#endif
#ifdef USE_LIBPNG
  libpng_decompress_thumbnails_done();
#endif
}

int
imo_decompress_thumbnail(struct image_obj *imo)
{
  return
    extract_image_info(imo) &&
    extract_thumb_data(imo) &&
    extract_thumb_image(imo);
}

