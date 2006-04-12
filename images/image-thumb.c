/*
 *	Image Library -- Thumbnails
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 *
 *	FIXME:
 *	- support for PNG (libpng seems to be fast)
 *	- improve thumbnail creation in gatherer... faster compression,
 *	  only grayscale/RGB colorspaces and maybe fixed headers (abbreviated datastreams in libjpeg)
 *	- hook memory allocation managers, get rid of multiple initializations
 *	- optimize decompression parameters
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/base224.h"
#include "lib/mempool.h"
#include "sherlock/object.h"
#include "images/images.h"
#include "images/image-thumb.h"

#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>

struct hook_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buf;
};

#define LONGJMP(cinfo) do{ longjmp(((struct hook_error_mgr *)(cinfo)->err)->setjmp_buf, 1); }while(0)

static void NONRET
hook_error_exit(j_common_ptr cinfo)
{ 
  LONGJMP(cinfo); 
}

static void
hook_init_source(j_decompress_ptr cinfo UNUSED)
{
}

static boolean NONRET
hook_fill_input_buffer(j_decompress_ptr cinfo)
{ 
  LONGJMP(cinfo);
}

static void
hook_skip_input_data(j_decompress_ptr cinfo, long num_bytes) 
{
  if (num_bytes > 0)
    {
      cinfo->src->next_input_byte += (size_t) num_bytes;
      cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
    }
}

int
decompress_thumbnail(struct odes *obj, struct mempool *pool, struct image *image)
{
  DBG("Decompressing thumbnail from URL %s", obj_find_aval(obj_find_attr(obj, 'U' + OBJ_ATTR_SON)->son, 'U'));
  
  /* Find the attribute */
  struct oattr *attr = obj_find_attr(obj, 'N');
  if (!attr)
    {
      DBG("There is no thumbnail");
      return -1;
    }

  /* Merge all instances of the attribute to continuous buffer */
  uns attr_count = 0;
  for (struct oattr *a = attr; a; a = a->same)
    attr_count++;
  byte b224[attr_count * MAX_ATTR_SIZE], *b = b224;
  for (struct oattr *a = attr; a; a = a->same)
    for (byte *s = a->val; *s; )
      *b++ = *s++;
  ASSERT(b != b224);

  /* Decode base-224 data */
  byte buf[b - b224];
  uns len = base224_decode(buf, b224, b - b224);
  DBG("Data length is %d", len);

  /* Initialize libjpeg decompression object */
  DBG("Creating libjpeg decompression object");
  struct jpeg_decompress_struct cinfo;
  struct hook_error_mgr err;
  cinfo.err = jpeg_std_error(&err.pub); // FIXME: we need to hide error messages
  err.pub.error_exit = hook_error_exit;
  if (setjmp(err.setjmp_buf))
    {
      DBG("Error during the decompression, longjump saved us");
      jpeg_destroy_decompress(&cinfo);
      return -2;
    }
  jpeg_create_decompress(&cinfo);

  /* Initialize source manager */
  struct jpeg_source_mgr src;
  cinfo.src = &src;
  src.next_input_byte = buf;
  src.bytes_in_buffer = len;
  src.init_source = hook_init_source;
  src.fill_input_buffer = hook_fill_input_buffer;
  src.skip_input_data = hook_skip_input_data;
  src.resync_to_restart = jpeg_resync_to_restart;
  src.term_source = hook_init_source;
  
  /* Read JPEG header and setup decompression options */
  DBG("Reading image header");
  jpeg_read_header(&cinfo, TRUE);
  image->flags = 0;
  if (cinfo.out_color_space == JCS_GRAYSCALE)
    image->flags |= IMAGE_GRAYSCALE;
  else
    cinfo.out_color_space = JCS_RGB;

  /* Decompress the image */
  DBG("Reading image data");
  jpeg_start_decompress(&cinfo);
  ASSERT(sizeof(JSAMPLE) == 1 && (
      (cinfo.out_color_space == JCS_GRAYSCALE && cinfo.output_components == 1) ||
      (cinfo.out_color_space == JCS_RGB && cinfo.output_components == 3)));
  image->width = cinfo.output_width;
  image->height = cinfo.output_height;
  uns scanline = cinfo.output_width * cinfo.output_components;
  uns size = scanline * cinfo.output_height;
  ASSERT(size);
  byte *pixels = image->pixels = mp_alloc(pool, size);
  while (cinfo.output_scanline < cinfo.output_height)
    {
      jpeg_read_scanlines(&cinfo, (JSAMPLE **)&pixels, 1);
      pixels += scanline;
    }
  jpeg_finish_decompress(&cinfo);

  /* Destroy libjpeg object and leave */
  jpeg_destroy_decompress(&cinfo);
  DBG("Thumbnail decompressed successfully");
  return 0;
}
