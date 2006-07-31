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
#include "images/io-main.h"
#include <stdio.h>
#include <sys/types.h>
#include <jpeglib.h>
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
  image_thread_err_dup(e->io->thread, IMAGE_ERR_READ_FAILED, buf);
  longjmp(e->setjmp_buf, 1);
}

static void NONRET
libjpeg_write_error_exit(j_common_ptr cinfo)
{
  DBG("libjpeg_error_exit()");
  struct libjpeg_err *e = (struct libjpeg_err *)cinfo->err;
  byte buf[JMSG_LENGTH_MAX];
  e->pub.format_message(cinfo, buf);
  image_thread_err_dup(e->io->thread, IMAGE_ERR_WRITE_FAILED,  buf);
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
  if (unlikely(msg_level == -1))
    longjmp(((struct libjpeg_err *)(cinfo)->err)->setjmp_buf, 1);
}

static inline uns
libjpeg_fastbuf_read_prepare(struct libjpeg_read_internals *i)
{
  byte *start;
  uns len = bdirect_read_prepare(i->fastbuf, &start);
  i->fastbuf_pos = start + len;
  i->src.next_input_byte = start;
  i->src.bytes_in_buffer = len;
  return len;
}

static inline void
libjpeg_fastbuf_read_commit(struct libjpeg_read_internals *i)
{
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
  return !!libjpeg_fastbuf_read_prepare(i);
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
	  bskip(i->fastbuf, num_bytes);
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
      image_thread_err(i->err.io->thread, IMAGE_ERR_WRITE_FAILED, "Unexpected end of stream");
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

  /* Read JPEG header and setup decompression options */
  DBG("Reading image header");
  jpeg_read_header(&i->cinfo, TRUE);
  switch (i->cinfo.jpeg_color_space)
    {
      case JCS_GRAYSCALE:
        io->flags = COLOR_SPACE_GRAYSCALE;
	io->number_of_colors = 1 << 8;
        break;
      default:
        io->flags = COLOR_SPACE_RGB;
	io->number_of_colors = 1 << 24;
        break;
    }
  io->cols = i->cinfo.image_width;
  io->rows = i->cinfo.image_height;

  io->read_cancel = libjpeg_read_cancel;
  return 1;
}

int
libjpeg_read_data(struct image_io *io)
{
  DBG("libjpeg_read_data()");

  struct libjpeg_read_internals *i = io->read_data;

  /* Select color space */
  switch (io->flags & IMAGE_COLOR_SPACE)
    {
      case COLOR_SPACE_GRAYSCALE:
	i->cinfo.out_color_space = JCS_GRAYSCALE;
	break;
      case COLOR_SPACE_RGB:
	i->cinfo.out_color_space = JCS_RGB;
	break;
      default:
	jpeg_destroy_decompress(&i->cinfo);
	image_thread_err(io->thread, IMAGE_ERR_INVALID_PIXEL_FORMAT, "Unsupported color space.");
	return 0;
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
  if (unlikely(!image_io_read_data_prepare(&rdi, io, i->cinfo.output_width, i->cinfo.output_height, io->flags)))
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
  switch (img->pixel_size)
    {
      /* grayscale or RGB */
      case 1:
      case 3:
	{
          byte *pixels = img->pixels;
	  for (uns r = img->rows; r--; )
            {
              jpeg_read_scanlines(&i->cinfo, (JSAMPLE **)&pixels, 1);
              pixels += img->row_size;
            }
	}
	break;
      /* grayscale with alpha */
      case 2:
	{
	  byte buf[img->cols], *src;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE img
#         define IMAGE_WALK_UNROLL 4
#         define IMAGE_WALK_COL_STEP 2
#         define IMAGE_WALK_DO_ROW_START do{ src = buf; jpeg_read_scanlines(&i->cinfo, (JSAMPLE **)&src, 1); }while(0)
#         define IMAGE_WALK_DO_STEP do{ walk_pos[0] = *src++; walk_pos[1] = 255; }while(0)
#         include "images/image-walk.h"
	}
	break;
      /* RGBA or aligned RGB */
      case 4:
	{
	  byte buf[img->cols * 3], *src;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE img
#         define IMAGE_WALK_UNROLL 4
#         define IMAGE_WALK_COL_STEP 4
#         define IMAGE_WALK_DO_ROW_START do{ src = buf; jpeg_read_scanlines(&i->cinfo, (JSAMPLE **)&src, 1); }while(0)
#         define IMAGE_WALK_DO_STEP do{ *(u32 *)walk_pos = *(u32 *)src; walk_pos[3] = 255; src += 3; }while(0)
#         include "images/image-walk.h"
	}
	break;
      default:
	ASSERT(0);
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
	i.cinfo.input_components = 1;
	i.cinfo.in_color_space = JCS_GRAYSCALE;
	break;
      case COLOR_SPACE_RGB:
	i.cinfo.input_components = 3;
	i.cinfo.in_color_space = JCS_RGB;
	break;
      default:
	jpeg_destroy_compress(&i.cinfo);
	image_thread_err(io->thread, IMAGE_ERR_INVALID_PIXEL_FORMAT, "Unsupported pixel format.");
	return 0;
    }
  jpeg_set_defaults(&i.cinfo);
  if (io->jpeg_quality)
    jpeg_set_quality(&i.cinfo, MIN(io->jpeg_quality, 100), 1);

  /* Compress the image */
  jpeg_start_compress(&i.cinfo, TRUE);
  switch (img->pixel_size)
    {
      /* grayscale or RGB */
      case 1:
      case 3:
	{
          byte *pixels = img->pixels;
	  for (uns r = img->rows; r--; )
	    {
              jpeg_write_scanlines(&i.cinfo, (JSAMPLE **)&pixels, 1);
              pixels += img->row_size;
            }
	}
	break;
      /* grayscale with alpha (ignore alpha) */
      case 2:
	{
	  byte buf[img->cols], *dest = buf;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE img
#         define IMAGE_WALK_UNROLL 4
#         define IMAGE_WALK_COL_STEP 2
#         define IMAGE_WALK_DO_ROW_END do{ dest = buf; jpeg_write_scanlines(&i.cinfo, (JSAMPLE **)&dest, 1); }while(0)
#         define IMAGE_WALK_DO_STEP do{ *dest++ = walk_pos[0]; }while(0)
#         include "images/image-walk.h"
	}
	break;
      /* RGBA (ignore alpha) or aligned RGB */
      case 4:
	{
	  byte buf[img->cols * 3], *dest = buf;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#         define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE img
#         define IMAGE_WALK_UNROLL 4
#         define IMAGE_WALK_COL_STEP 4
#         define IMAGE_WALK_DO_ROW_END do{ dest = buf; jpeg_write_scanlines(&i.cinfo, (JSAMPLE **)&dest, 1); }while(0)
#         define IMAGE_WALK_DO_STEP do{ *dest++ = walk_pos[0]; *dest++ = walk_pos[1]; *dest++ = walk_pos[2]; }while(0)
#         include "images/image-walk.h"
	}
	break;
      default:
	ASSERT(0);
    }
  ASSERT(i.cinfo.next_scanline == i.cinfo.image_height);
  jpeg_finish_compress(&i.cinfo);
  jpeg_destroy_compress(&i.cinfo);
  return 1;
}
