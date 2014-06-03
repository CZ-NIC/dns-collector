/*
 *	Image Library -- libungif
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/fastbuf.h>
#include <images/images.h>
#include <images/error.h>
#include <images/color.h>
#include <images/io-main.h>

#include <gif_lib.h>

struct libungif_read_data {
  GifFileType *gif;
  int transparent_index;
};

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

  struct libungif_read_data *rd = io->read_data;
  DGifCloseFile(rd->gif);
}

int
libungif_read_header(struct image_io *io)
{
  DBG("libungif_read_header()");

  /* Create libungif structure */
  GifFileType *gif;
  if (unlikely(!(gif = DGifOpen(io->fastbuf, libungif_read_func))))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Cannot create libungif structure.");
      return 0;
    }

  struct libungif_read_data *rd = io->read_data = mp_alloc(io->internal_pool, sizeof(*rd));
  rd->gif = gif;

  DBG("executing DGifSlurp()");
  if (unlikely(DGifSlurp(gif) != GIF_OK))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Gif read failed.");
      DGifCloseFile(gif);
      return 0;
    }

  DBG("ImageCount=%d ColorResolution=%d SBackGroundColor=%d SColorMap=%p", gif->ImageCount, gif->SColorResolution, gif->SBackGroundColor, gif->SColorMap);
  if (unlikely(!gif->ImageCount))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "There are no images in gif file.");
      DGifCloseFile(gif);
      return 0;
    }

  /* Read image parameters */
  SavedImage *image = gif->SavedImages;
  if (unlikely(image->ImageDesc.Width <= 0 || image->ImageDesc.Height <= 0 ||
      image->ImageDesc.Width > (int)image_max_dim || image->ImageDesc.Height > (int)image_max_dim))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_INVALID_DIMENSIONS, "Invalid gif dimensions.");
      DGifCloseFile(gif);
      return 0;
    }
  ColorMapObject *color_map = image->ImageDesc.ColorMap ? : gif->SColorMap;
  if (unlikely(!color_map))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Missing palette.");
      DGifCloseFile(gif);
      return 0;
    }
  io->cols = image->ImageDesc.Width;
  io->rows = image->ImageDesc.Height;
  if (unlikely((io->number_of_colors = color_map->ColorCount) > 256))
    {
      IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Too many gif colors.");
      DGifCloseFile(gif);
      return 0;
    }
  io->flags = COLOR_SPACE_RGB | IMAGE_IO_HAS_PALETTE;

  /* Search extension blocks */
  rd->transparent_index = -1;
  for (int i = 0; i < image->ExtensionBlockCount; i++)
    {
      ExtensionBlock *e = image->ExtensionBlocks + i;
      if (e->Function == 0xF9)
        {
	  DBG("Found graphics control extension");
	  if (unlikely(e->ByteCount != 4))
	    {
              IMAGE_ERROR(io->context, IMAGE_ERROR_READ_FAILED, "Invalid graphics control extension.");
              DGifCloseFile(gif);
              return 0;
	    }
	  byte *b = e->Bytes;
	  /* transparent color present */
	  if (b[0] & 1)
	    {
	      rd->transparent_index = b[3];
	      io->flags |= IMAGE_ALPHA;
	      if (gif->SColorMap)
	        {
                  GifColorType *background = color_map->Colors + gif->SBackGroundColor;
                  color_make_rgb(&io->background_color, background->Red, background->Green, background->Blue);
		}
	    }
	  /* We've got everything we need :-) */
	  break;
	}
      else
	DBG("Found unknown extension: type=%d size=%d", e->Function, e->ByteCount);
    }

  /* Success */
  io->read_cancel = libungif_read_cancel;
  return 1;
}

int
libungif_read_data(struct image_io *io)
{
  DBG("libungif_read_data()");

  struct libungif_read_data *rd = io->read_data;
  GifFileType *gif = rd->gif;
  SavedImage *image = gif->SavedImages;

  /* Prepare image */
  struct image_io_read_data_internals rdi;
  uint read_flags = io->flags;
  uint cs = read_flags & IMAGE_COLOR_SPACE;
  if (cs != COLOR_SPACE_GRAYSCALE && cs != COLOR_SPACE_RGB)
    read_flags = (read_flags & ~IMAGE_COLOR_SPACE & IMAGE_CHANNELS_FORMAT) | COLOR_SPACE_RGB;
  if (unlikely(!image_io_read_data_prepare(&rdi, io, image->ImageDesc.Width, image->ImageDesc.Height, read_flags)))
    {
      DGifCloseFile(gif);
      return 0;
    }

  /* Get pixels and palette */
  byte *pixels = (byte *)image->RasterBits;
  ColorMapObject *color_map = image->ImageDesc.ColorMap ? : gif->SColorMap;
  GifColorType *palette = color_map->Colors;
  byte *img_end = rdi.image->pixels + rdi.image->image_size;

  /* Handle deinterlacing */
  uint dein_step, dein_next;
  if (image->ImageDesc.Interlace)
    {
      DBG("Deinterlaced image");
      dein_step = dein_next = rdi.image->row_size << 3;
    }
  else
    dein_step = dein_next = rdi.image->row_size;

  /* Convert pixels */
  switch (rdi.image->pixel_size)
    {
      case 1:
	{
	  byte pal[256], *pal_pos = pal, *pal_end = pal + 256;
	  for (uint i = 0; i < (uint)color_map->ColorCount; i++, pal_pos++, palette++)
	    *pal_pos = rgb_to_gray_func(palette->Red, palette->Green, palette->Blue);
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (rd->transparent_index >= 0 && (io->flags & IMAGE_IO_USE_BACKGROUND))
	    if (!color_put(io->context, &io->background_color, pal + rd->transparent_index, COLOR_SPACE_GRAYSCALE))
	      {
		DGifCloseFile(gif);
		return 0;
	      }
#	  define DO_ROW_END do{ \
	      walk_row_start += dein_step; \
	      while (walk_row_start >= img_end) \
		{ uint n = dein_next >> 1; walk_row_start = rdi.image->pixels + n, dein_step = dein_next; dein_next = n; } \
	    }while(0)
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 1
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ *walk_pos = pal[*pixels++]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include <images/image-walk.h>
	  break;
	}
      case 2:
	{
	  byte pal[256 * 2], *pal_pos = pal, *pal_end = pal + 256 * 2;
	  for (uint i = 0; i < (uint)color_map->ColorCount; i++, pal_pos += 2, palette++)
	    {
	      pal_pos[0] = rgb_to_gray_func(palette->Red, palette->Green, palette->Blue);
	      pal_pos[1] = 255;
	    }
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (rd->transparent_index >= 0)
	    pal[rd->transparent_index * 2 + 1] = 0;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 2
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ *(u16 *)walk_pos = ((u16 *)pal)[*pixels++]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include <images/image-walk.h>
	  break;
	}
      case 3:
	{
	  byte pal[256 * 4], *pal_pos = pal, *pal_end = pal + 256 * 4;
	  for (uint i = 0; i < (uint)color_map->ColorCount; i++, pal_pos += 4, palette++)
	    {
	      pal_pos[0] = palette->Red;
	      pal_pos[1] = palette->Green;
	      pal_pos[2] = palette->Blue;
	    }
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (rd->transparent_index >= 0 && (io->flags & IMAGE_IO_USE_BACKGROUND))
	    if (!color_put(io->context, &io->background_color, pal + 4 * rd->transparent_index, COLOR_SPACE_RGB))
	      {
		DGifCloseFile(gif);
		return 0;
	      }
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 3
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ byte *p = pal + 4 * (*pixels++); walk_pos[0] = p[0]; walk_pos[1] = p[1]; walk_pos[2] = p[2]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include <images/image-walk.h>
	  break;
	}
      case 4:
	{
	  byte pal[256 * 4], *pal_pos = pal, *pal_end = pal + 256 * 4;
	  for (uint i = 0; i < (uint)color_map->ColorCount; i++, pal_pos += 4, palette++)
	    {
	      pal_pos[0] = palette->Red;
	      pal_pos[1] = palette->Green;
	      pal_pos[2] = palette->Blue;
	      pal_pos[3] = 255;
	    }
	  if (pal_pos != pal_end)
	    bzero(pal_pos, pal_end - pal_pos);
	  if (rd->transparent_index >= 0)
	    pal[rd->transparent_index * 4 + 3] = 0;
#	  define IMAGE_WALK_PREFIX(x) walk_##x
#	  define IMAGE_WALK_INLINE
#	  define IMAGE_WALK_IMAGE (rdi.image)
#	  define IMAGE_WALK_UNROLL 4
#	  define IMAGE_WALK_COL_STEP 4
#	  define IMAGE_WALK_ROW_STEP 0
#	  define IMAGE_WALK_DO_STEP do{ *(u32 *)walk_pos = ((u32 *)pal)[*pixels++]; }while(0)
#	  define IMAGE_WALK_DO_ROW_END DO_ROW_END
#	  include <images/image-walk.h>
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
