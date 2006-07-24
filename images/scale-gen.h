/*
 *	Image Library -- Image scaling algorithms
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef IMAGE_SCALE_CHANNELS
#  define IMAGE_SCALE_CHANNELS IMAGE_SCALE_PIXEL_SIZE
#endif

static void
IMAGE_SCALE_PREFIX(downsample)(struct image *dest, struct image *src)
{
  /* FIXME slow */
  byte *rsrc = src->pixels, *psrc;
  byte *rdest = dest->pixels, *pdest;
  uns x_inc = (dest->cols << 16) / src->cols, x_pos, x_inc_frac = 0xffffff / x_inc;
  uns y_inc = (dest->rows << 16) / src->rows, y_pos = 0, y_inc_frac = 0xffffff / y_inc;
  uns final_mul = ((u64)x_inc * y_inc) >> 16;
  uns buf_size = dest->cols * IMAGE_SCALE_CHANNELS;
  u32 buf[buf_size], *pbuf;
  buf_size *= sizeof(u32);
  bzero(buf, buf_size);
  for (uns rows_counter = src->rows; rows_counter--; )
    {
      pbuf = buf;
      psrc = rsrc;
      rsrc += src->row_size;
      x_pos = 0;
      y_pos += y_inc;
      if (y_pos <= 0x10000)
        {
          for (uns cols_counter = src->cols; cols_counter--; )
            {
	      x_pos += x_inc;
	      if (x_pos <= 0x10000)
	        {
	          pbuf[0] += psrc[0];
#		  if IMAGE_SCALE_CHANNELS >= 2
	          pbuf[1] += psrc[1];
#                 endif
#		  if IMAGE_SCALE_CHANNELS >= 3
	          pbuf[2] += psrc[2];
#                 endif
#		  if IMAGE_SCALE_CHANNELS >= 4
	          pbuf[3] += psrc[3];
#                 endif
	        }
	      else
	        {
	          x_pos -= 0x10000;
	          uns mul2 = x_pos * x_inc_frac;
	          uns mul1 = 0xffffff - mul2;
	          pbuf[0] += (psrc[0] * mul1) >> 24;
	          pbuf[0 + IMAGE_SCALE_CHANNELS] += (psrc[0] * mul2) >> 24;
#		  if IMAGE_SCALE_CHANNELS >= 2
	          pbuf[1] += (psrc[1] * mul1) >> 24;
	          pbuf[1 + IMAGE_SCALE_CHANNELS] += (psrc[1] * mul2) >> 24;
#                 endif
#		  if IMAGE_SCALE_CHANNELS >= 3
	          pbuf[2] += (psrc[2] * mul1) >> 24;
	          pbuf[2 + IMAGE_SCALE_CHANNELS] += (psrc[2] * mul2) >> 24;
#                 endif
#		  if IMAGE_SCALE_CHANNELS >= 4
	          pbuf[3] += (psrc[3] * mul1) >> 24;
	          pbuf[3 + IMAGE_SCALE_CHANNELS] += (psrc[3] * mul2) >> 24;
#                 endif
	          pbuf += IMAGE_SCALE_CHANNELS;
	        }
	      psrc += IMAGE_SCALE_PIXEL_SIZE;
	    }
	}
      else
        {
	  y_pos -= 0x10000;
          pdest = rdest;
          rdest += dest->row_size;
	  uns mul2 = y_pos * y_inc_frac;
	  uns mul1 = 0xffffff - mul2;
	  uns a0 = 0;
#	  if IMAGE_SCALE_CHANNELS >= 2
	  uns a1 = 0;
#	  endif
#	  if IMAGE_SCALE_CHANNELS >= 3
	  uns a2 = 0;
#	  endif
#	  if IMAGE_SCALE_CHANNELS >= 4
	  uns a3 = 0;
#	  endif
          for (uns cols_counter = src->cols; cols_counter--; )
            {
	      x_pos += x_inc;
	      if (x_pos <= 0x10000)
	        {
		  pbuf[0] += ((psrc[0] * mul1) >> 24);
		  a0 += (psrc[0] * mul2) >> 24;
#	          if IMAGE_SCALE_CHANNELS >= 2
		  pbuf[1] += ((psrc[1] * mul1) >> 24);
		  a1 += (psrc[1] * mul2) >> 24;
#                 endif
#	          if IMAGE_SCALE_CHANNELS >= 3
		  pbuf[2] += ((psrc[2] * mul1) >> 24);
		  a2 += (psrc[2] * mul2) >> 24;
#                 endif
#	          if IMAGE_SCALE_CHANNELS >= 4
		  pbuf[3] += ((psrc[3] * mul1) >> 24);
		  a3 += (psrc[3] * mul2) >> 24;
#                 endif
	        }
	      else
	        {
	          x_pos -= 0x10000;
		  uns mul4 = x_pos * x_inc_frac;
		  uns mul3 = 0xffffff - mul4;
		  uns mul13 = ((u64)mul1 * mul3) >> 24;
		  uns mul23 = ((u64)mul2 * mul3) >> 24;
		  uns mul14 = ((u64)mul1 * mul4) >> 24;
		  uns mul24 = ((u64)mul2 * mul4) >> 24;
		  pdest[0] = ((((psrc[0] * mul13) >> 24) + pbuf[0]) * final_mul) >> 16;
		  pbuf[0] = ((psrc[0] * mul23) >> 24) + a0;
		  pbuf[0 + IMAGE_SCALE_CHANNELS] += ((psrc[0 + IMAGE_SCALE_PIXEL_SIZE] * mul14) >> 24);
		  a0 = ((psrc[0 + IMAGE_SCALE_PIXEL_SIZE] * mul24) >> 24);
#	          if IMAGE_SCALE_CHANNELS >= 2
		  pdest[1] = ((((psrc[1] * mul13) >> 24) + pbuf[1]) * final_mul) >> 16;
		  pbuf[1] = ((psrc[1] * mul23) >> 24) + a1;
		  pbuf[1 + IMAGE_SCALE_CHANNELS] += ((psrc[1 + IMAGE_SCALE_PIXEL_SIZE] * mul14) >> 24);
		  a1 = ((psrc[1 + IMAGE_SCALE_PIXEL_SIZE] * mul24) >> 24);
#                 endif
#	          if IMAGE_SCALE_CHANNELS >= 3
		  pdest[2] = ((((psrc[2] * mul13) >> 24) + pbuf[2]) * final_mul) >> 16;
		  pbuf[2] = ((psrc[2] * mul23) >> 24) + a2;
		  pbuf[2 + IMAGE_SCALE_CHANNELS] += ((psrc[2 + IMAGE_SCALE_PIXEL_SIZE] * mul14) >> 24);
		  a2 = ((psrc[2 + IMAGE_SCALE_PIXEL_SIZE] * mul24) >> 24);
#                 endif
#	          if IMAGE_SCALE_CHANNELS >= 4
		  pdest[3] = ((((psrc[3] * mul13) >> 24) + pbuf[3]) * final_mul) >> 16;
		  pbuf[3] = ((psrc[3] * mul23) >> 24) + a3;
		  pbuf[3 + IMAGE_SCALE_CHANNELS] += ((psrc[3 + IMAGE_SCALE_PIXEL_SIZE] * mul14) >> 24);
		  a3 = ((psrc[3 + IMAGE_SCALE_PIXEL_SIZE] * mul24) >> 24);
#                 endif
	          pbuf += IMAGE_SCALE_CHANNELS;
		  pdest += IMAGE_SCALE_PIXEL_SIZE;
	        }
	      psrc += IMAGE_SCALE_PIXEL_SIZE;
	    }
	  pdest[0] = (pbuf[0] * final_mul) >> 16;
	  pbuf[0] = a0;
#         if IMAGE_SCALE_CHANNELS >= 2
	  pdest[1] = (pbuf[1] * final_mul) >> 16;
	  pbuf[1] = a1;
#	  endif
#         if IMAGE_SCALE_CHANNELS >= 3
	  pdest[2] = (pbuf[2] * final_mul) >> 16;
	  pbuf[2] = a2;
#	  endif
#         if IMAGE_SCALE_CHANNELS >= 4
	  pdest[3] = (pbuf[3] * final_mul) >> 16;
	  pbuf[3] = a3;
#	  endif
	}
    }
  pdest = rdest;
  pbuf = buf;
  for (uns cols_counter = dest->cols; cols_counter--; )
    {
      pdest[0] = (pbuf[0] * final_mul) >> 16;
#     if IMAGE_SCALE_CHANNELS >= 2
      pdest[1] = (pbuf[1] * final_mul) >> 16;
#     endif
#     if IMAGE_SCALE_CHANNELS >= 3
      pdest[2] = (pbuf[2] * final_mul) >> 16;
#     endif
#     if IMAGE_SCALE_CHANNELS >= 4
      pdest[3] = (pbuf[3] * final_mul) >> 16;
#     endif
      pbuf += IMAGE_SCALE_CHANNELS;
      pdest += IMAGE_SCALE_PIXEL_SIZE;
    }
}

#undef IMAGE_SCALE_PREFIX
#undef IMAGE_SCALE_PIXEL_SIZE
#undef IMAGE_SCALE_CHANNELS
