/*
 *	Image Library -- libjpeg
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

#include <stdio.h>
#include <sys/types.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

struct libjpeg_err {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buf;
  struct image_io *io;
};

struct libjpeg_read_internals {
  struct jpeg_decompress_struct cinfo;
  struct jpeg_source_mgr src;
  struct libjpeg_err err;
  struct fastbuf *fastbuf;
  byte *fastbuf_pos;
};

struct libjpeg_write_internals {
  struct jpeg_compress_struct cinfo;
  struct jpeg_destination_mgr dest;
  struct libjpeg_err err;
  struct fastbuf *fastbuf;
  byte *fastbuf_pos;
};

static void NONRET
libjpeg_read_error_exit(j_common_ptr cinfo)
{
  DBG("libjpeg_error_exit()");
  struct libjpeg_err *e = (struct libjpeg_err *)cinfo->err;
  byte buf[JMSG_LENGTH_MAX];
  e->pub.format_message(cinfo, buf);
  IMAGE_ERROR(e->io->context, IMAGE_ERROR_READ_FAILED, "libjpeg: %s", buf);
  longjmp(e->setjmp_buf, 1);
}

static void NONRET
libjpeg_write_error_exit(j_common_ptr cinfo)
{
  DBG("libjpeg_error_exit()");
  struct libjpeg_err *e = (struct libjpeg_err *)cinfo->err;
  byte buf[JMSG_LENGTH_MAX];
  e->pub.format_message(cinfo, buf);
  IMAGE_ERROR(e->io->context, IMAGE_ERROR_WRITE_FAILED, "libjpeg: %s", buf);
  longjmp(e->setjmp_buf, 1);
}

static void
libjpeg_emit_message(j_common_ptr cinfo UNUSED, int msg_level UNUSED)
{
#ifdef LOCAL_DEBUG
  byte buf[JMSG_LENGTH_MAX];
  cinfo->err->format_message(cinfo, buf);
  DBG("libjpeg_emit_message(): [%d] %s", msg_level, buf);
#endif
#if 0
  // Terminate on warning?
  if (unlikely(msg_level == -1))
    {
      struct libjpeg_err *e = (struct libjpeg_err *)cinfo->err;
      byte buf[JMSG_LENGTH_MAX];
      cinfo->err->format_message(cinfo, buf);
      IMAGE_ERROR(e->io->context, 0, "libjpeg: %s", buf);
      longjmp(e->setjmp_buf, 1);
    }
#endif
}

static inline uns
libjpeg_fastbuf_read_prepare(struct libjpeg_read_internals *i)
{
  DBG("libjpeg_fb_read_prepare()");
  byte *start;
  uns len = bdirect_read_prepare(i->fastbuf, &start);
  DBG("readed %u bytes at %p", len, start);
  if (!len)
    {
      // XXX: maybe only generate a warning and return EOI markers to recover from such errors (also in skip_input_data)
      IMAGE_ERROR(i->err.io->context, IMAGE_ERROR_READ_FAILED, "Incomplete JPEG file");
      longjmp(i->err.setjmp_buf, 1);
    }
  i->fastbuf_pos = start + len;
  i->src.next_input_byte = start;
  i->src.bytes_in_buffer = len;
  return len;
}

static inline void
libjpeg_fastbuf_read_commit(struct libjpeg_read_internals *i)
{
  DBG("libjpeg_fb_read_commit()");
  bdirect_read_commit(i->fastbuf, i->fastbuf_pos);
}

static void
libjpeg_init_source(j_decompress_ptr cinfo)
{
  DBG("libjpeg_init_source()");
  libjpeg_fastbuf_read_prepare((struct libjpeg_read_internals *)cinfo);
}

static void
libjpeg_term_source(j_decompress_ptr cinfo UNUSED)
{
  DBG("libjpeg_term_source()");
  //libjpeg_fastbuf_read_commit((struct libjpeg_read_internals *)cinfo);
}

static boolean
libjpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
  DBG("libjpeg_fill_input_buffer()");
  struct libjpeg_read_internals *i = (struct libjpeg_read_internals *)cinfo;
  libjpeg_fastbuf_read_commit(i);
  libjpeg_fastbuf_read_prepare(i);
  return 1;
}

static void
libjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  DBG("libjpeg_skip_input_data(num_bytes=%d)", (int)num_bytes);
  if (num_bytes > 0)
    {
      struct libjpeg_read_internals *i = (struct libjpeg_read_internals *)cinfo;
      if ((unsigned long)num_bytes <= i->src.bytes_in_buffer)
        {
          i->src.next_input_byte += num_bytes;
          i->src.bytes_in_buffer -= num_bytes;
	}
      else
        {
	  num_bytes -= i->src.bytes_in_buffer;
	  libjpeg_fastbuf_read_commit(i);
	  if (!bskip(i->fastbuf, num_bytes))
	    {
	      IMAGE_ERROR(i->err.io->context, IMAGE_ERROR_READ_FAILED, "Incomplete JPEG file");
	      longjmp(i->err.setjmp_buf, 1);
	    }
	  libjpeg_fastbuf_read_prepare(i);
	}
    }
}

static inline void
libjpeg_fastbuf_write_prepare(struct libjpeg_write_internals *i)
{
  byte *start;
  uns len = bdirect_write_prepare(i->fastbuf, &start);
  i->fastbuf_pos = start + len;
  i->dest.next_output_byte = start;
  i->dest.free_in_buffer = len;
  if (!len)
    {
      IMAGE_ERROR(i->err.io->context, IMAGE_ERROR_WRITE_FAILED, "Unexpected end of stream");
      longjmp(i->err.setjmp_buf, 1);
    }
}

static void
libjpeg_init_destination(j_compress_ptr cinfo)
{
  DBG("libjpeg_init_destination()");
  libjpeg_fastbuf_write_prepare((struct libjpeg_write_internals *)cinfo);
}

static void
libjpeg_term_destination(j_compress_ptr cinfo)
{
  DBG("libjpeg_term_destination()");
  struct libjpeg_write_internals *i = (struct libjpeg_write_internals *)cinfo;
  bdirect_write_commit(i->fastbuf, (byte *)i->dest.next_output_byte);
}

static boolean
libjpeg_empty_output_buffer(j_compress_ptr cinfo)
{
  DBG("libjpeg_empty_output_buffer()");
  struct libjpeg_write_internals *i = (struct libjpeg_write_internals *)cinfo;
  bdirect_write_commit(i->fastbuf, i->fastbuf_pos);
  libjpeg_fastbuf_write_prepare(i);
  return TRUE;
}

static inline uns
libjpeg_read_byte(struct libjpeg_read_internals *i)
{
  DBG("libjpeg_read_byte()");
  if (!i->src.bytes_in_buffer)
    if (!libjpeg_fill_input_buffer(&i->cinfo))
      ERREXIT(&i->cinfo, JERR_CANT_SUSPEND);
  i->src.bytes_in_buffer--;
  return *i->src.next_input_byte++;
}

static inline void
libjpeg_read_buf(struct libjpeg_read_internals *i, byte *buf, uns len)
{
  DBG("libjpeg_read_buf(len=%u)", len);
  while (len)
    {
      if (!i->src.bytes_in_buffer)
	if (!libjpeg_fill_input_buffer(&i->cinfo))
	  ERREXIT(&i->cinfo, JERR_CANT_SUSPEND);
      uns buf_size = i->src.bytes_in_buffer;
      uns read_size = MIN(buf_size, len);
      memcpy(buf, i->src.next_input_byte, read_size);
      i->src.bytes_in_buffer -= read_size;
      i->src.next_input_byte += read_size;
      len -= read_size;
    }
}

static byte libjpeg_exif_header[6] = { 'E', 'x', 'i', 'f', 0, 0 };

static boolean
libjpeg_app1_preprocessor(j_decompress_ptr cinfo)
{
  struct libjpeg_read_internals *i = (struct libjpeg_read_internals *)cinfo;
  struct image_io *io = i->err.io;
  uns len = libjpeg_read_byte(i) << 8;
  len += libjpeg_read_byte(i);
  DBG("Found APP1 marker, len=%u", len);
  if (len < 2)
    return TRUE;
  len -= 2;
  if (len < 7 /*|| io->exif_size*/)
    {
      libjpeg_skip_input_data(cinfo, len);
      return TRUE;
    }
  byte header[6];
  libjpeg_read_buf(i, header, 6);
  if (memcmp(header, libjpeg_exif_header, 6))
    {
      libjpeg_skip_input_data(cinfo, len - 6);
      return TRUE;
    }
  io->exif_size = len;
  io->exif_data = mp_alloc(io->internal_pool, len);
  memcpy(io->exif_data, header, 6);
  libjpeg_read_buf(i, io->exif_data + 6, len - 6);
  DBG("Parsed EXIF of length %u", len);
  return TRUE;
}

static void
libjpeg_read_cancel(struct image_io *io)
{
  DBG("libjpeg_read_cancel()");
  struct libjpeg_read_internals *i = io->read_data;
  jpeg_destroy_decompress(&i->cinfo);
}

int
libjpeg_read_header(struct image_io *io)
{
  DBG("libjpeg_read_header()");
  struct libjpeg_read_internals *i = io->read_data = mp_alloc(io->internal_pool, sizeof(*i));
  i->fastbuf = io->fastbuf;

  /* Create libjpeg read structure */
  DBG("Creating libjpeg read structure");
  i->cinfo.err = jpeg_std_error(&i->err.pub);
  i->err.pub.error_exit = libjpeg_read_error_exit;
  i->err.pub.emit_message = libjpeg_emit_message;
  i->err.io = io;
  if (setjmp(i->err.setjmp_buf))
    {
      DBG("Libjpeg failed to read the image, longjump saved us");
      jpeg_destroy_decompress(&i->cinfo);
      return 0;
    }
  jpeg_create_decompress(&i->cinfo);

  /* Initialize source manager */
  i->cinfo.src = &i->src;
  i->src.init_source = libjpeg_init_source;
  i->src.fill_input_buffer = libjpeg_fill_input_buffer;
  i->src.skip_input_data = libjpeg_skip_input_data;
  i->src.resync_to_restart = jpeg_resync_to_restart;
  i->src.term_source = libjpeg_term_source;

  if (io->flags & IMAGE_IO_WANT_EXIF)
    jpeg_set_marker_processor(&i->cinfo, JPEG_APP0 + 1, libjpeg_app1_preprocessor);

  /* Read JPEG header and setup decompression options */
  DBG("Reading image header");
  jpeg_read_header(&i->cinfo, TRUE);
  switch (i->cinfo.jpeg_color_space)
    {
      case JCS_GRAYSCALE:
        io->flags = COLOR_SPACE_GRAYSCALE;
        break;
      case JCS_RGB:
	io->flags = COLOR_SPACE_RGB;
	break;
      case JCS_YCbCr:
	io->flags = COLOR_SPACE_YCBCR;
	break;
      case JCS_CMYK:
	io->flags = COLOR_SPACE_CMYK;
	break;
      case JCS_YCCK:
	io->flags = COLOR_SPACE_YCCK;
	break;
      default:
	if (unlikely(i->cinfo.num_components < 1 || i->cinfo.num_components > 4))
	  {
	    jpeg_destroy_decompress(&i->cinfo);
	    IMAGE_ERROR(io->context, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Invalid color space.");
	    return 0;
	  }
	io->flags = COLOR_SPACE_UNKNOWN + i->cinfo.num_components;
	break;
    }
  if (unlikely(i->cinfo.num_components != (int)color_space_channels[io->flags]))
    {
      jpeg_destroy_decompress(&i->cinfo);
      IMAGE_ERROR(io->context, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Invalid number of color channels.");
      return 0;
    }
  io->cols = i->cinfo.image_width;
  io->rows = i->cinfo.image_height;
  io->number_of_colors = (i->cinfo.num_components < 4) ? (1U << (i->cinfo.num_components * 8)) : 0xffffffff;
  io->read_cancel = libjpeg_read_cancel;
  return 1;
}

int
libjpeg_read_data(struct image_io *io)
{
  DBG("libjpeg_read_data()");

  struct libjpeg_read_internals *i = io->read_data;
  uns read_flags = io->flags;

  /* Select color space */
  switch (read_flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
	i->cinfo.out_color_space = JCS_GRAYSCALE;
	break;
      case COLOR_SPACE_YCBCR:
	i->cinfo.out_color_space = JCS_YCbCr;
	break;
      case COLOR_SPACE_CMYK:
	i->cinfo.out_color_space = JCS_CMYK;
	break;
      case COLOR_SPACE_YCCK:
	i->cinfo.out_color_space = JCS_YCCK;
	break;
      default:
	switch (i->cinfo.jpeg_color_space)
	  {
	    case JCS_CMYK:
	      read_flags = (read_flags & ~IMAGE_COLOR_SPACE & IMAGE_CHANNELS_FORMAT) | COLOR_SPACE_CMYK; 
	      i->cinfo.out_color_space = JCS_CMYK;
	      break;
	    case JCS_YCCK:
	      read_flags = (read_flags & ~IMAGE_COLOR_SPACE & IMAGE_CHANNELS_FORMAT) | COLOR_SPACE_YCCK; 
	      i->cinfo.out_color_space = JCS_YCCK;
	      break;
	    default:
	      read_flags = (read_flags & ~IMAGE_COLOR_SPACE & IMAGE_CHANNELS_FORMAT) | COLOR_SPACE_RGB; 
	      i->cinfo.out_color_space = JCS_RGB;
	      break;
	  }
	break;
    }

  /* Prepare the image  */
  struct image_io_read_data_internals rdi;
  if (io->cols <= (i->cinfo.image_width >> 3) && io->rows <= (i->cinfo.image_height >> 3))
    {
      DBG("Scaling to 1/8");
      i->cinfo.scale_num = 1;
      i->cinfo.scale_denom = 8;
    }
  else if (io->cols <= (i->cinfo.image_width >> 2) && io->rows <= (i->cinfo.image_height >> 2))
    {
      DBG("Scaling to 1/4");
      i->cinfo.scale_num = 1;
      i->cinfo.scale_denom = 4;
    }
  else if (io->cols <= (i->cinfo.image_width >> 1) && io->rows <= (i->cinfo.image_height >> 1))
    {
      DBG("Scaling to 1/2");
      i->cinfo.scale_num = 1;
      i->cinfo.scale_denom = 2;
    }
  jpeg_calc_output_dimensions(&i->cinfo);
  DBG("Output dimensions %ux%u", (uns)i->cinfo.output_width, (uns)i->cinfo.output_height);
  if (unlikely(!image_io_read_data_prepare(&rdi, io, i->cinfo.output_width, i->cinfo.output_height, read_flags)))
    {
      jpeg_destroy_decompress(&i->cinfo);
      return 0;
    }

  /* Setup fallback */
  if (setjmp(i->err.setjmp_buf))
    {
      DBG("Libjpeg failed to read the image, longjump saved us");
      jpeg_destroy_decompress(&i->cinfo);
      image_io_read_data_break(&rdi, io);
      return 0;
    }

  /* Decompress the image */
  struct image *img = rdi.image;
  jpeg_start_decompress(&i->cinfo);
  if ((int)img->pixel_size == i->cinfo.output_components)
    {
      byte *pixels = img->pixels;
      for (uns r = img->rows; r--; )
        {
          jpeg_read_scanlines(&i->cinfo, (JSAMPLE **)&pixels, 1);
          pixels += img->row_size;
        }
    }
  else
    {
      switch (img->pixel_size)
        {
	  case 2: /* Grayscale -> Grayscale+Alpha */
	    {
	      ASSERT(i->cinfo.output_components == 1);
	      byte buf[img->cols], *src;
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#             define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_IMAGE img
#             define IMAGE_WALK_UNROLL 4
#             define IMAGE_WALK_COL_STEP 2
#             define IMAGE_WALK_DO_ROW_START do{ src = buf; jpeg_read_scanlines(&i->cinfo, (JSAMPLE **)&src, 1); }while(0)
#             define IMAGE_WALK_DO_STEP do{ walk_pos[0] = *src++; walk_pos[1] = 255; }while(0)
#             include "images/image-walk.h"
	    }
	    break;
	  case 4: /* * -> *+Alpha or aligned * */
	    {
	      ASSERT(i->cinfo.output_components == 3);
	      byte buf[img->cols * 3], *src;
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#             define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_IMAGE img
#             define IMAGE_WALK_UNROLL 4
#             define IMAGE_WALK_COL_STEP 4
#             define IMAGE_WALK_DO_ROW_START do{ src = buf; jpeg_read_scanlines(&i->cinfo, (JSAMPLE **)&src, 1); }while(0)
#             define IMAGE_WALK_DO_STEP do{ *(u32 *)walk_pos = *(u32 *)src; walk_pos[3] = 255; src += 3; }while(0)
#             include "images/image-walk.h"
	    }
	    break;
	  default:
	    ASSERT(0);
	}

    }

  ASSERT(i->cinfo.output_scanline == i->cinfo.output_height);

  /* Destroy libjpeg object */
  jpeg_finish_decompress(&i->cinfo);
  jpeg_destroy_decompress(&i->cinfo);

  /* Finish the image */
  return image_io_read_data_finish(&rdi, io);
}

int
libjpeg_write(struct image_io *io)
{
  DBG("libjpeg_write()");
  struct libjpeg_write_internals i;
  i.fastbuf = io->fastbuf;

  /* Create libjpeg write structure */
  DBG("Creating libjpeg write structure");
  i.cinfo.err = jpeg_std_error(&i.err.pub);
  i.err.pub.error_exit = libjpeg_write_error_exit;
  i.err.pub.emit_message = libjpeg_emit_message;
  i.err.io = io;
  if (setjmp(i.err.setjmp_buf))
    {
      DBG("Libjpeg failed to write the image, longjump saved us");
      jpeg_destroy_compress(&i.cinfo);
      return 0;
    }
  jpeg_create_compress(&i.cinfo);

  /* Initialize destination manager */
  i.cinfo.dest = &i.dest;
  i.dest.init_destination = libjpeg_init_destination;
  i.dest.term_destination = libjpeg_term_destination;
  i.dest.empty_output_buffer = libjpeg_empty_output_buffer;

  /* Set output parameters */
  struct image *img = io->image;
  i.cinfo.image_width = img->cols;
  i.cinfo.image_height = img->rows;
  switch (img->flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
	i.cinfo.in_color_space = JCS_GRAYSCALE;
	break;
      case COLOR_SPACE_RGB:
	i.cinfo.in_color_space = JCS_RGB;
	break;
      case COLOR_SPACE_YCBCR:
	i.cinfo.in_color_space = JCS_YCbCr;
	break;
      case COLOR_SPACE_CMYK:
	i.cinfo.in_color_space = JCS_CMYK;
	break;
      case COLOR_SPACE_YCCK:
	i.cinfo.in_color_space = JCS_YCCK;
	break;
      default:
	jpeg_destroy_compress(&i.cinfo);
	IMAGE_ERROR(io->context, IMAGE_ERROR_INVALID_PIXEL_FORMAT, "Unsupported pixel format.");
	return 0;
    }
  i.cinfo.input_components = color_space_channels[img->flags & IMAGE_COLOR_SPACE];
  jpeg_set_defaults(&i.cinfo);
  jpeg_set_colorspace(&i.cinfo, i.cinfo.in_color_space);
  if (io->jpeg_quality)
    jpeg_set_quality(&i.cinfo, MIN(io->jpeg_quality, 100), 1);
  if (io->exif_size)
    {
      /* According to the Exif specification, the Exif APP1 marker has to follow immediately after the SOI,
       * just as the JFIF specification requires the same for the JFIF APP0 marker!
       * Therefore a JPEG file cannot legally be both Exif and JFIF.  */
      i.cinfo.write_JFIF_header = FALSE;
      i.cinfo.write_Adobe_marker = FALSE;
    }

  /* Compress the image */
  jpeg_start_compress(&i.cinfo, TRUE);
  if (io->exif_size)
    {
      DBG("Writing EXIF");
      jpeg_write_marker(&i.cinfo, JPEG_APP0 + 1, io->exif_data, io->exif_size);
    }
  if ((int)img->pixel_size == i.cinfo.input_components)
    {
      byte *pixels = img->pixels;
      for (uns r = img->rows; r--; )
        {
          jpeg_write_scanlines(&i.cinfo, (JSAMPLE **)&pixels, 1);
          pixels += img->row_size;
        }
    }
  else
    {
      switch (img->pixel_size)
        {
	  case 2: /* Grayscale+Alpha -> Grayscale */
	    {
	      ASSERT(i.cinfo.input_components == 1);
	      byte buf[img->cols], *dest = buf;
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#             define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_IMAGE img
#             define IMAGE_WALK_UNROLL 4
#             define IMAGE_WALK_COL_STEP 2
#             define IMAGE_WALK_DO_ROW_END do{ dest = buf; jpeg_write_scanlines(&i.cinfo, (JSAMPLE **)&dest, 1); }while(0)
#             define IMAGE_WALK_DO_STEP do{ *dest++ = walk_pos[0]; }while(0)
#             include "images/image-walk.h"
	      break;
	    }
	  case 4: /* *+Alpha or aligned * -> * */
	    {
	      ASSERT(i.cinfo.input_components == 3);
	      byte buf[img->cols * 3], *dest = buf;
#	      define IMAGE_WALK_PREFIX(x) walk_##x
#             define IMAGE_WALK_INLINE
#	      define IMAGE_WALK_IMAGE img
#             define IMAGE_WALK_UNROLL 4
#             define IMAGE_WALK_COL_STEP 4
#             define IMAGE_WALK_DO_ROW_END do{ dest = buf; jpeg_write_scanlines(&i.cinfo, (JSAMPLE **)&dest, 1); }while(0)
#             define IMAGE_WALK_DO_STEP do{ *dest++ = walk_pos[0]; *dest++ = walk_pos[1]; *dest++ = walk_pos[2]; }while(0)
#             include "images/image-walk.h"
	      break;
	    }
	  default:
	    ASSERT(0);
	}
    }
  ASSERT(i.cinfo.next_scanline == i.cinfo.image_height);
  jpeg_finish_compress(&i.cinfo);
  jpeg_destroy_compress(&i.cinfo);
  return 1;
}
