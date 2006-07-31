/*
 *	Image Library -- libungif
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
#include "images/color.h"
#include "images/io-main.h"
#include <gif_lib.h>

static int
libungif_read_func(GifFileType *gif, GifByteType *ptr, int len)
{
  DBG("libungif_read_func(len=%d)", len);
  return bread((struct fastbuf *)gif->UserData, (byte *)ptr, len);
}

static void
libungif_read_cancel(struct image_io *io)
{
  DBG("libungif_read_cancel()");

  DGifCloseFile(io->read_data);
}

int
libungif_read_header(struct image_io *io)
{
  DBG("libungif_read_header()");

  /* Create libungif structure */
  GifFileType *gif;
  if (unlikely(!(gif = io->read_data = DGifOpen(io->fastbuf, libungif_read_func))))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Cannot create libungif structure.");
      return 0;
    }

  DBG("executing DGifSlurp()");
  if (unlikely(DGifSlurp(gif) != GIF_OK))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Gif read failed.");
      DGifCloseFile(gif);
      return 0;
    }

  DBG("ImageCount=%d ColorResolution=%d SBackGroundColor=%d SColorMap=%p", gif->ImageCount, gif->SColorResolution, gif->SBackGroundColor, gif->SColorMap);
  if (unlikely(!gif->ImageCount))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "There are no images in gif file.");
      DGifCloseFile(gif);
      return 0;
    }

  /* Read image parameters */
  SavedImage *image = gif->SavedImages;
  if (unlikely(image->ImageDesc.Width <= 0 || image->ImageDesc.Height <= 0 ||
      image->ImageDesc.Width > (int)IMAGE_MAX_SIZE || image->ImageDesc.Height > (int)IMAGE_MAX_SIZE))
    {
      image_thread_err(io->thread, IMAGE_ERR_INVALID_DIMENSIONS, "Invalid gif dimensions.");
      DGifCloseFile(gif);
      return 0;
    }
  ColorMapObject *color_map = image->ImageDesc.ColorMap ? : gif->SColorMap;
  if (unlikely(!color_map))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Missing palette.");
      DGifCloseFile(gif);
      return 0;
    }
  io->cols = image->ImageDesc.Width;
  io->rows = image->ImageDesc.Height;
  if (unlikely((io->number_of_colors = color_map->ColorCount) > 256))
    {
      image_thread_err(io->thread, IMAGE_ERR_READ_FAILED, "Too many gif colors.");
      DGifCloseFile(gif);
      return 0;
    }
  io->flags = COLOR_SPACE_RGB | IMAGE_IO_HAS_PALETTE;
  /* FIXME transparent GIFs disabled */
#if 0
  if (gif->SColorMap && !image->ImageDesc.ColorMap && (uns)gif->SBackGroundColor < (uns)color_map->ColorCount)
    {
      io->flags |= IMAGE_ALPHA | IMAGE_IO_HAS_BACKGROUND;
      GifColorType *background = color_map->Colors + gif->SBackGroundColor;
      color_make_rgb(&io->background_color, background->Red, background->Green, background->Blue);
    }
#endif

  /* Success */
  io->read_cancel = libungif_read_cancel;
  return 1;
}

int
libungif_read_data(struct image_io *io)
{
  DBG("libungif_read_data()");

  GifFileType *gif = io->read_data;
  SavedImage *image = gif->SavedImages;

  /* Prepare image */
  struct image_io_read_data_internals rdi;
  if (unlikely(!image_io_read_data_prepare(&rdi, io, image->ImageDesc.Width, image->ImageDesc.Height, io->flags)))
    {
      DGifCloseFile(gif);
      return 0;
    }

  /* Get pixels and palette */
  byte *pixels = (byte *)image->RasterBits;
  ColorMapObject *color_map = image->ImageDesc.ColorMap ? : gif->SColorMap;
  GifColorType *palette = color_map->Colors;
  uns background = (
  /* FIXME: transparent GIFs disabled */
#if 1
    0 &&
#endif
    gif->SColorMap && !image->ImageDesc.ColorMap) ? gif->SBackGroundColor : 256;
  byte *img_end = rdi.image->pixels + rdi.image->image_size;

  /* Handle deinterlacing */
  uns dein_step, dein_next;
  if (image->ImageDesc.Interlace)
    dein_step = dein_next = rdi.image->row_size << 3;
  else
    dein_step = dein_next = rdi.image->row_size;

  /* Convert pixels */
  switch (rdi.image->pixel_size)
    {
      case 1:
	{
	  byte pal[256], *pal_pos = pal, *pal_end = pal + 256;
	  for (uns i = 0; i < (uns)color_map->ColorCount; i++, pal_pos++, palette++)
	    *pal_pos = rgb_to_gray_func(palette->Red, palette->Green, palette->Blue);
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (io->flags & IMAGE_IO_USE_BACKGROUND)
	    color_put_grayscale(pal + background, &io->background_color);
#	  define DO_ROW_END do{ \
  	      walk_row_start += dein_step; \
  	      if (walk_row_start > img_end) \
		{ uns n = dein_next >> 1; walk_row_start = rdi.image->pixels + n, dein_step = dein_next; dein_next = n; } \
	    }while(0)
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 1
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ *walk_pos = pal[*pixels++]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include "images/image-walk.h"
	  break;
	}
      case 2:
	{
	  byte pal[256 * 2], *pal_pos = pal, *pal_end = pal + 256 * 2;
	  for (uns i = 0; i < (uns)color_map->ColorCount; i++, pal_pos += 2, palette++)
	    {
	      pal_pos[0] = rgb_to_gray_func(palette->Red, palette->Green, palette->Blue);
	      pal_pos[1] = 255;
	    }
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (background < 256)
	    pal[background * 2 + 1] = 0;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 2
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ *(u16 *)walk_pos = ((u16 *)pal)[*pixels++]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include "images/image-walk.h"
	  break;
	}
      case 3:
	{
	  byte pal[256 * 4], *pal_pos = pal, *pal_end = pal + 256 * 4;
	  for (uns i = 0; i < (uns)color_map->ColorCount; i++, pal_pos += 4, palette++)
	    {
	      pal_pos[0] = palette->Red;
	      pal_pos[1] = palette->Green;
	      pal_pos[2] = palette->Blue;
	    }
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (io->flags & IMAGE_IO_USE_BACKGROUND)
	    color_put_rgb(pal + background, &io->background_color);
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 3
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ byte *p = pal + 4 * (*pixels++); walk_pos[0] = p[0]; walk_pos[1] = p[1]; walk_pos[2] = p[2]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include "images/image-walk.h"
	  break;
	}
      case 4:
	{
	  byte pal[256 * 4], *pal_pos = pal, *pal_end = pal + 256 * 4;
	  for (uns i = 0; i < (uns)color_map->ColorCount; i++, pal_pos += 4, palette++)
	    {
	      pal_pos[0] = palette->Red;
	      pal_pos[1] = palette->Green;
	      pal_pos[2] = palette->Blue;
	      pal_pos[3] = 255;
	    }
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (background < 256)
	    pal[background * 4 + 3] = 0;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 4
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ *(u32 *)walk_pos = ((u32 *)pal)[*pixels++]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include "images/image-walk.h"
	  break;
	}
      default:
        ASSERT(0);
    }

  /* Destroy libungif structure */
  DGifCloseFile(gif);

  /* Finish image */
  return image_io_read_data_finish(&rdi, io);
}
