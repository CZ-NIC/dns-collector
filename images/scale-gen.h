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

#undef IMAGE_COPY_PIXEL
#if IMAGE_SCALE_PIXEL_SIZE == 1
#define IMAGE_COPY_PIXEL(dest, src) do{ *(byte *)dest = *(byte *)src; }while(0)
#elif IMAGE_SCALE_PIXEL_SIZE == 2
#define IMAGE_COPY_PIXEL(dest, src) do{ *(u16 *)dest = *(u16 *)src; }while(0)
#elif IMAGE_SCALE_PIXEL_SIZE == 3
#define IMAGE_COPY_PIXEL(dest, src) do{ ((byte *)dest)[0] = ((byte *)src)[0]; ((byte *)dest)[1] = ((byte *)src)[1]; ((byte *)dest)[2] = ((byte *)src)[2]; }while(0)
#elif IMAGE_SCALE_PIXEL_SIZE == 4
#define IMAGE_COPY_PIXEL(dest, src) do{ *(u32 *)dest = *(u32 *)src; }while(0)
#endif

static void
IMAGE_SCALE_PREFIX(nearest_xy)(struct image *dest, struct image *src)
{
  uns x_inc = (src->cols << 16) / dest->cols;
  uns y_inc = (src->rows << 16) / dest->rows;
  uns x_start = x_inc >> 1, x_pos;
  uns y_pos = y_inc >> 1;
  byte *row_start;
# define IMAGE_WALK_PREFIX(x) walk_##x
# define IMAGE_WALK_INLINE
# define IMAGE_WALK_UNROLL 4
# define IMAGE_WALK_IMAGE dest
# define IMAGE_WALK_COL_STEP IMAGE_SCALE_PIXEL_SIZE
# define IMAGE_WALK_DO_ROW_START do{ row_start = src->pixels + (y_pos >> 16) * src->row_size; y_pos += y_inc; x_pos = x_start; }while(0)
# define IMAGE_WALK_DO_STEP do{ byte *pos = row_start + (x_pos >> 16) * IMAGE_SCALE_PIXEL_SIZE; x_pos += x_inc; IMAGE_COPY_PIXEL(walk_pos, pos); }while(0)
# include "images/image-walk.h"
}

#if 0 /* Experiments with rearranging pixels for SSE... */
static void
IMAGE_SCALE_PREFIX(linear_x)(struct image *dest, struct image *src)
{
  /* Handle problematic special case */
  byte *src_row = src->pixels;
  byte *dest_row = dest->pixels;
  if (src->cols == 1)
    {
      for (uns y_counter = dest->rows; y_counter--; )
        {
          // FIXME
          ASSERT(0);
	  src_row += src->row_size;
	  dest_row += dest->row_size;
	}
      return;
    }
  /* Initialize the main loop */
  uns x_inc = ((src->cols - 1) << 16) / (dest->cols - 1);
# define COLS_AT_ONCE 256
  byte pixel_buf[COLS_AT_ONCE * 2 * IMAGE_SCALE_PIXEL_SIZE]; /* Buffers should fit in cache */
  u16 coef_buf[COLS_AT_ONCE * IMAGE_SCALE_PIXEL_SIZE];
  /* Main loop */
  for (uns y_counter = dest->rows; y_counter--; )
    {
      uns x_pos = 0;
      byte *dest_pos = dest_row;
      for (uns x_counter = dest->cols; --x_counter; )
      for (uns x_counter = dest->cols; x_counter > COLS_AT_ONCE; x_counter -= COLS_AT_ONCE)
        {
	  byte *pixel_buf_pos = pixel_buf;
	  u16 *coef_buf_pos = coef_buf;
	  for (uns i = 0; i < COLS_AT_ONCE / 2; i++)
	    {
	      byte *src_pos = src_row + (x_pos >> 16) * IMAGE_SCALE_PIXEL_SIZE;
	      uns ofs = x_pos & 0xffff;
	      x_pos += x_inc;
	      byte *src_pos_2 = src_row + (x_pos >> 16) * IMAGE_SCALE_PIXEL_SIZE;
	      uns ofs_2 = x_pos & 0xffff;
	      x_pos += x_inc;
	      *coef_buf_pos++ = ofs;
	      byte *pixel_buf_pos_2 = pixel_buf_pos + IMAGE_SCALE_PIXEL_SIZE;
	      byte *pixel_buf_pos_3 = pixel_buf_pos + IMAGE_SCALE_PIXEL_SIZE * 2;
	      byte *pixel_buf_pos_4 = pixel_buf_pos + IMAGE_SCALE_PIXEL_SIZE * 3;
              IMAGE_COPY_PIXEL(pixel_buf_pos, src_pos);
	      IMAGE_COPY_PIXEL(pixel_buf_pos_2, src_pos + IMAGE_SCALE_PIXEL_SIZE);
              IMAGE_COPY_PIXEL(pixel_buf_pos_3, src_pos_2);
	      IMAGE_COPY_PIXEL(pixel_buf_pos_4, src_pos_2 + IMAGE_SCALE_PIXEL_SIZE);
	      pixel_buf_pos += 4 * IMAGE_SCALE_PIXEL_SIZE;
	      *coef_buf_pos++ = ofs_2;
	    }
/*
	  byte *src_pos = src_row + (x_pos >> 16) * IMAGE_SCALE_PIXEL_SIZE;
	  uns ofs = x_pos & 0xffff;
	  x_pos += x_inc;
	  dest_pos[0] = LINEAR_INTERPOLATE(src_pos[0], src_pos[0 + IMAGE_SCALE_PIXEL_SIZE], ofs);
#	  if IMAGE_SCALE_CHANNELS >= 2
	  dest_pos[1] = LINEAR_INTERPOLATE(src_pos[1], src_pos[1 + IMAGE_SCALE_PIXEL_SIZE], ofs);
#	  endif
#	  if IMAGE_SCALE_CHANNELS >= 3
	  dest_pos[2] = LINEAR_INTERPOLATE(src_pos[2], src_pos[2 + IMAGE_SCALE_PIXEL_SIZE], ofs);
#	  endif
#	  if IMAGE_SCALE_CHANNELS >= 4
	  dest_pos[3] = LINEAR_INTERPOLATE(src_pos[3], src_pos[3 + IMAGE_SCALE_PIXEL_SIZE], ofs);
#	  endif
	  dest_pos += IMAGE_SCALE_PIXEL_SIZE;*/

	}
      /* Always copy the last column - handle "x_pos == dest->cols * 0x10000" overflow */
      IMAGE_COPY_PIXEL(dest_pos, src_row + src->row_pixels_size - IMAGE_SCALE_PIXEL_SIZE);
      /* Next step */
      src_row += src->row_size;
      dest_row += dest->row_size;
    }
#undef COLS_AT_ONCE
}

static void
IMAGE_SCALE_PREFIX(bilinear_xy)(struct image *dest, struct image *src)
{
  uns x_inc = (((src->cols - 1) << 16) - 1) / (dest->cols);
  uns y_inc = (((src->rows - 1) << 16) - 1) / (dest->rows);
  uns y_pos = 0x10000;
  byte *cache[2], buf1[dest->row_pixels_size + 16], buf2[dest->row_pixels_size + 16], *pbuf[2];
  byte *dest_row = dest->pixels, *dest_pos;
  uns cache_index = ~0U, cache_i = 0;
  pbuf[0] = cache[0] = ALIGN_PTR((void *)buf1, 16);
  pbuf[1] = cache[1] = ALIGN_PTR((void *)buf2, 16);
#ifdef __SSE2__
  __m128i zero = _mm_setzero_si128();
#endif
  for (uns row_counter = dest->rows; row_counter--; )
    {
      dest_pos = dest_row;
      uns y_index = y_pos >> 16;
      uns y_ofs = y_pos & 0xffff;
      y_pos += y_inc;
      uns x_pos = 0;
      if (y_index > (uns)(cache_index + 1))
	cache_index = y_index - 1;
      while (y_index > cache_index)
        {
	  cache[0] = cache[1];
	  cache[1] = pbuf[cache_i ^= 1];
	  cache_index++;
	  byte *src_row = src->pixels + cache_index * src->row_size;
	  byte *cache_pos = cache[1];
	  for (uns col_counter = dest->cols; --col_counter; )
	    {
	      byte *c1 = src_row + (x_pos >> 16) * IMAGE_SCALE_PIXEL_SIZE;
	      byte *c2 = c1 + IMAGE_SCALE_PIXEL_SIZE;
	      uns ofs = x_pos & 0xffff;
	      cache_pos[0] = LINEAR_INTERPOLATE(c1[0], c2[0], ofs);
#	      if IMAGE_SCALE_CHANNELS >= 2
	      cache_pos[1] = LINEAR_INTERPOLATE(c1[1], c2[1], ofs);
#	      endif
#	      if IMAGE_SCALE_CHANNELS >= 3
	      cache_pos[2] = LINEAR_INTERPOLATE(c1[2], c2[2], ofs);
#	      endif
#	      if IMAGE_SCALE_CHANNELS >= 4
	      cache_pos[3] = LINEAR_INTERPOLATE(c1[3], c2[3], ofs);
#	      endif
	      cache_pos += IMAGE_SCALE_PIXEL_SIZE;
	      x_pos += x_inc;
	    }
	  IMAGE_COPY_PIXEL(cache_pos, src_row + src->row_pixels_size - IMAGE_SCALE_PIXEL_SIZE);
	}
      uns i = 0;
#ifdef __SSE2__
      __m128i coef = _mm_set1_epi16(y_ofs >> 9);
      for (; (int)i < (int)dest->row_pixels_size - 15; i += 16)
        {
	  __m128i a2 = _mm_loadu_si128((__m128i *)(cache[0] + i));
	  __m128i a1 = _mm_unpacklo_epi8(a2, zero);
	  a2 = _mm_unpackhi_epi8(a2, zero);
	  __m128i b2 = _mm_loadu_si128((__m128i *)(cache[1] + i));
	  __m128i b1 = _mm_unpacklo_epi8(b2, zero);
	  b2 = _mm_unpackhi_epi8(b2, zero);
	  b1 = _mm_sub_epi16(b1, a1);
	  b2 = _mm_sub_epi16(b2, a2);
	  a1 = _mm_slli_epi16(a1, 7);
	  a2 = _mm_slli_epi16(a2, 7);
	  b1 = _mm_mullo_epi16(b1, coef);
	  b2 = _mm_mullo_epi16(b2, coef);
	  a1 = _mm_add_epi16(a1, b1);
	  a2 = _mm_add_epi16(a2, b2);
	  a1 = _mm_srli_epi16(a1, 7);
	  a2 = _mm_srli_epi16(a2, 7);
	  a1 = _mm_packus_epi16(a1, a2);
	  _mm_storeu_si128((__m128i *)(dest_pos + i), a1);
	}
#elif 1
      for (; (int)i < (int)dest->row_pixels_size - 3; i += 4)
        {
	  dest_pos[i + 0] = LINEAR_INTERPOLATE(cache[0][i + 0], cache[1][i + 0], y_ofs);
	  dest_pos[i + 1] = LINEAR_INTERPOLATE(cache[0][i + 1], cache[1][i + 1], y_ofs);
	  dest_pos[i + 2] = LINEAR_INTERPOLATE(cache[0][i + 2], cache[1][i + 2], y_ofs);
	  dest_pos[i + 3] = LINEAR_INTERPOLATE(cache[0][i + 3], cache[1][i + 3], y_ofs);
	}
#endif
      for (; i < dest->row_pixels_size; i++)
	dest_pos[i] = LINEAR_INTERPOLATE(cache[0][i], cache[1][i], y_ofs);
      dest_row += dest->row_size;
    }
}
#endif

static void
IMAGE_SCALE_PREFIX(downsample_xy)(struct image *dest, struct image *src)
{
  /* FIXME slow */
  byte *rsrc = src->pixels, *psrc;
  byte *rdest = dest->pixels, *pdest;
  u64 x_inc = ((u64)dest->cols << 32) / src->cols, x_pos;
  u64 y_inc = ((u64)dest->rows << 32) / src->rows, y_pos = 0;
  uns x_inc_frac = (u64)0xffffffffff / x_inc;
  uns y_inc_frac = (u64)0xffffffffff / y_inc;
  uns final_mul = ((u64)(x_inc >> 16) * (y_inc >> 16)) >> 16;
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
	      if (x_pos <= 0x100000000)
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
	          x_pos -= 0x100000000;
	          uns mul2 = (uns)(x_pos >> 16) * x_inc_frac;
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
	  y_pos -= 0x100000000;
          pdest = rdest;
          rdest += dest->row_size;
	  uns mul2 = (uns)(y_pos >> 16) * y_inc_frac;
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
	      if (x_pos <= 0x100000000)
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
	          x_pos -= 0x100000000;
		  uns mul4 = (uns)(x_pos >> 16) * x_inc_frac;
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
