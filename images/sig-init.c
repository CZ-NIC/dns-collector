/*
 *	Image Library -- Computation of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/conf.h>
#include <images/images.h>
#include <images/math.h>
#include <images/error.h>
#include <images/color.h>
#include <images/signature.h>

#include <alloca.h>
#include <math.h>

int
image_sig_init(struct image_context *ctx, struct image_sig_data *data, struct image *image)
{
  ASSERT((image->flags & IMAGE_PIXEL_FORMAT) == COLOR_SPACE_RGB);
  data->image = image;
  data->flags = 0;
  data->cols = (image->cols + 3) >> 2;
  data->rows = (image->rows + 3) >> 2;
  data->full_cols = image->cols >> 2;
  data->full_rows = image->rows >> 2;
  data->blocks_count = data->cols * data->rows;
  if (data->blocks_count >= 0x10000)
    {
      IMAGE_ERROR(ctx, IMAGE_ERROR_INVALID_DIMENSIONS, "Image too large for implemented signature algorithm.");
      return 0;
    }
  data->blocks = xmalloc(data->blocks_count * sizeof(struct image_sig_block));
  data->area = image->cols * image->rows;
  DBG("Computing signature for image of %ux%u pixels (%ux%u blocks)",
      image->cols, image->rows, data->cols, data->rows);
  return 1;
}

void
image_sig_preprocess(struct image_sig_data *data)
{
  struct image *image = data->image;
  struct image_sig_block *block = data->blocks;
  uns sum[IMAGE_VEC_F];
  bzero(sum, sizeof(sum));

  /* Every block of 4x4 pixels */
  byte *row_start = image->pixels;
  for (uns block_y = 0; block_y < data->rows; block_y++, row_start += image->row_size * 4)
    {
      byte *p = row_start;
      for (uns block_x = 0; block_x < data->cols; block_x++, p += 12, block++)
        {
          int t[16], s[16], *tp = t;
	  block->x = block_x;
	  block->y = block_y;

	  /* Convert pixels to Luv color space and compute average coefficients */
	  uns l_sum = 0, u_sum = 0, v_sum = 0;
	  byte *p2 = p;
	  if (block_x < data->full_cols && block_y < data->full_rows)
	    {
	      for (uns y = 0; y < 4; y++, p2 += image->row_size - 12)
	        for (uns x = 0; x < 4; x++, p2 += 3)
	          {
	            byte luv[3];
	            srgb_to_luv_pixel(luv, p2);
	            l_sum += *tp++ = luv[0] / 4;
	            u_sum += luv[1];
	            v_sum += luv[2];
	          }
	      block->area = 16;
	      sum[0] += l_sum;
	      sum[1] += u_sum;
	      sum[2] += v_sum;
	      block->v[0] = (l_sum >> 4);
	      block->v[1] = (u_sum >> 4);
	      block->v[2] = (v_sum >> 4);
	    }
	  /* Incomplete square near the edge */
	  else
	    {
	      uns x, y;
	      uns square_cols = (block_x < data->full_cols) ? 4 : image->cols & 3;
	      uns square_rows = (block_y < data->full_rows) ? 4 : image->rows & 3;
	      for (y = 0; y < square_rows; y++, p2 += image->row_size)
	        {
		  byte *p3 = p2;
	          for (x = 0; x < square_cols; x++, p3 += 3)
	            {
	              byte luv[3];
	              srgb_to_luv_pixel(luv, p3);
	              l_sum += *tp++ = luv[0] / 4;
	              u_sum += luv[1];
	              v_sum += luv[2];
		    }
		  for (; x < 4; x++)
		    {
		      *tp = tp[-(int)square_cols];
		      tp++;
		    }
	        }
	      for (; y < 4; y++)
	        for (x = 0; x < 4; x++)
	          {
	            *tp = tp[-(int)square_rows * 4];
		    tp++;
		  }
	      block->area = square_cols * square_rows;
	      uns inv = 0x10000 / block->area;
	      sum[0] += l_sum;
	      sum[1] += u_sum;
	      sum[2] += v_sum;
	      block->v[0] = (l_sum * inv) >> 16;
	      block->v[1] = (u_sum * inv) >> 16;
	      block->v[2] = (v_sum * inv) >> 16;
	    }

	  /* Apply Daubechies wavelet transformation */

#         define DAUB_0	31651	/* (1 + sqrt 3) / (4 * sqrt 2) * 0x10000 */
#         define DAUB_1	54822	/* (3 + sqrt 3) / (4 * sqrt 2) * 0x10000 */
#         define DAUB_2	14689	/* (3 - sqrt 3) / (4 * sqrt 2) * 0x10000 */
#         define DAUB_3	-8481	/* (1 - sqrt 3) / (4 * sqrt 2) * 0x10000 */

	  /* ... to the rows */
	  uns i;
          for (i = 0; i < 16; i += 4)
            {
	      s[i + 0] = (DAUB_0 * t[i + 2] + DAUB_1 * t[i + 3] + DAUB_2 * t[i + 0] + DAUB_3 * t[i + 1]) / 0x10000;
	      s[i + 1] = (DAUB_0 * t[i + 0] + DAUB_1 * t[i + 1] + DAUB_2 * t[i + 2] + DAUB_3 * t[i + 3]) / 0x10000;
	      s[i + 2] = (DAUB_3 * t[i + 2] - DAUB_2 * t[i + 3] + DAUB_1 * t[i + 0] - DAUB_0 * t[i + 1]) / 0x10000;
	      s[i + 3] = (DAUB_3 * t[i + 0] - DAUB_2 * t[i + 1] + DAUB_1 * t[i + 2] - DAUB_0 * t[i + 3]) / 0x10000;
	    }

	  /* ... and to the columns... skip LL band */
	  for (i = 0; i < 2; i++)
	    {
	      t[i + 8] = (DAUB_3 * s[i + 8] - DAUB_2 * s[i +12] + DAUB_1 * s[i + 0] - DAUB_0 * s[i + 4]) / 0x10000;
	      t[i +12] = (DAUB_3 * s[i + 0] - DAUB_2 * s[i + 4] + DAUB_1 * s[i + 8] - DAUB_0 * s[i +12]) / 0x10000;
	    }
	  for (; i < 4; i++)
	    {
	      t[i + 0] = (DAUB_0 * s[i + 8] + DAUB_1 * s[i +12] + DAUB_2 * s[i + 0] + DAUB_3 * s[i + 4]) / 0x10000;
	      t[i + 4] = (DAUB_0 * s[i + 0] + DAUB_1 * s[i + 4] + DAUB_2 * s[i + 8] + DAUB_3 * s[i +12]) / 0x10000;
	      t[i + 8] = (DAUB_3 * s[i + 8] - DAUB_2 * s[i +12] + DAUB_1 * s[i + 0] - DAUB_0 * s[i + 4]) / 0x10000;
	      t[i +12] = (DAUB_3 * s[i + 0] - DAUB_2 * s[i + 4] + DAUB_1 * s[i + 8] - DAUB_0 * s[i +12]) / 0x10000;
	    }

	  /* Extract energies in LH, HL and HH bands */
	  block->v[3] = fast_sqrt_u32(isqr(t[8]) + isqr(t[9]) + isqr(t[12]) + isqr(t[13]));
	  block->v[4] = fast_sqrt_u32(isqr(t[2]) + isqr(t[3]) + isqr(t[6]) + isqr(t[7]));
	  block->v[5] = fast_sqrt_u32(isqr(t[10]) + isqr(t[11]) + isqr(t[14]) + isqr(t[15]));
	  sum[3] += block->v[3] * block->area;
	  sum[4] += block->v[4] * block->area;
	  sum[5] += block->v[5] * block->area;
        }
    }

  /* Compute featrures average */
  uns inv = 0xffffffffU / data->area;
  for (uns i = 0; i < IMAGE_VEC_F; i++)
    data->f[i] = ((u64)sum[i] * inv) >> 32;

  if (image->cols < image_sig_min_width || image->rows < image_sig_min_height)
    {
      data->valid = 0;
      data->regions_count = 0;
    }
  else
    data->valid = 1;
}

void
image_sig_finish(struct image_sig_data *data, struct image_signature *sig)
{
  for (uns i = 0; i < IMAGE_VEC_F; i++)
    sig->vec.f[i] = data->f[i];
  sig->len = data->regions_count;
  sig->flags = data->flags;
  if (!sig->len)
    return;

  /* For each region */
  u64 w_total = 0;
  uns w_border = MIN(data->cols, data->rows) * image_sig_border_size;
  int w_mul = w_border ? image_sig_border_bonus * 256 / (int)w_border : 0;
  for (uns i = 0; i < sig->len; i++)
    {
      struct image_sig_region *r = data->regions + i;
      DBG("Processing region %u: count=%u", i, r->count);
      ASSERT(r->count);

      /* Copy texture properties */
      sig->reg[i].f[0] = r->a[0];
      sig->reg[i].f[1] = r->a[1];
      sig->reg[i].f[2] = r->a[2];
      sig->reg[i].f[3] = r->a[3];
      sig->reg[i].f[4] = r->a[4];
      sig->reg[i].f[5] = r->a[5];

      /* Compute coordinates centroid and region weight */
      u64 x_sum = 0, y_sum = 0, w_sum = 0;
      for (struct image_sig_block *b = r->blocks; b; b = b->next)
        {
	  x_sum += b->x;
	  y_sum += b->y;
	  uns d = b->x;
	  d = MIN(d, b->y);
	  d = MIN(d, data->cols - b->x - 1);
	  d = MIN(d, data->rows - b->y - 1);
	  if (d >= w_border)
	    w_sum += 128;
	  else
	    w_sum += 128 + (int)(w_border - d) * w_mul / 256;
	}
      w_total += w_sum;
      r->w_sum = w_sum;
      uns x_avg = x_sum / r->count;
      uns y_avg = y_sum / r->count;
      DBG("  centroid=(%u %u)", x_avg, y_avg);

      /* Compute normalized inertia */
      u64 sum1 = 0, sum2 = 0, sum3 = 0;
      for (struct image_sig_block *b = r->blocks; b; b = b->next)
        {
	  uns inc2 = isqr(x_avg - b->x) + isqr(y_avg - b->y);
	  uns inc1 = fast_sqrt_u32(inc2);
	  sum1 += inc1;
	  sum2 += inc2;
	  sum3 += inc1 * inc2;
	}
      sig->reg[i].h[0] = CLAMP(image_sig_inertia_scale[0] * sum1 * ((3 * M_PI * M_PI) / 2) * pow(r->count, -1.5), 0, 255);
      sig->reg[i].h[1] = CLAMP(image_sig_inertia_scale[1] * sum2 * ((4 * M_PI * M_PI * M_PI) / 2) / ((u64)r->count * r->count), 0, 255);
      sig->reg[i].h[2] = CLAMP(image_sig_inertia_scale[2] * sum3 * ((5 * M_PI * M_PI * M_PI * M_PI) / 2) * pow(r->count, -2.5), 0, 255);
      sig->reg[i].h[3] = (uns)x_avg * 127 / data->cols;
      sig->reg[i].h[4] = (uns)y_avg * 127 / data->rows;
    }

  /* Compute average differences */
  u64 df = 0, dh = 0;

  if (sig->len < 2)
    {
      sig->df = 1;
      sig->dh = 1;
    }
  else
    {
      uns cnt = 0;
      for (uns i = 0; i < sig->len; i++)
        for (uns j = i + 1; j < sig->len; j++)
          {
	    uns d = 0;
	    for (uns k = 0; k < IMAGE_REG_F; k++)
	      d += image_sig_cmp_features_weights[k] * isqr(sig->reg[i].f[k] - sig->reg[j].f[k]);
	    df += fast_sqrt_u32(d);
	    d = 0;
	    for (uns k = 0; k < IMAGE_REG_H; k++)
	      d += image_sig_cmp_features_weights[k + IMAGE_REG_F] * isqr(sig->reg[i].h[k] - sig->reg[j].h[k]);
	    dh += fast_sqrt_u32(d);
	    cnt++;
          }
      sig->df = CLAMP(df / cnt, 1, 0xffff);
      sig->dh = CLAMP(dh / cnt, 1, 0xffff);
    }
  DBG("Average regions difs: df=%u dh=%u", sig->df, sig->dh);

  /* Compute normalized weights */
  uns wa = 128, wb = 128;
  for (uns i = sig->len; --i > 0; )
    {
      struct image_sig_region *r = data->regions + i;
      wa -= sig->reg[i].wa = CLAMP(r->count * 128 / data->blocks_count, 1, (int)(wa - i));
      wb -= sig->reg[i].wb = CLAMP(r->w_sum * 128 / w_total, 1, (int)(wb - i));
    }
  sig->reg[0].wa = wa;
  sig->reg[0].wb = wb;

  /* Store image dimensions */
  sig->cols = data->image->cols;
  sig->rows = data->image->rows;

  /* Dump regions features */
#ifdef LOCAL_DEBUG
  for (uns i = 0; i < sig->len; i++)
    {
      byte buf[IMAGE_REGION_DUMP_MAX];
      image_region_dump(buf, sig->reg + i);
      DBG("region %u: features=%s", i, buf);
    }
#endif
}

void
image_sig_cleanup(struct image_sig_data *data)
{
  xfree(data->blocks);
}

int
compute_image_signature(struct image_context *ctx, struct image_signature *sig, struct image *image)
{
  struct image_sig_data data;
  if (!image_sig_init(ctx, &data, image))
    return 0;
  image_sig_preprocess(&data);
  if (data.valid)
    {
      image_sig_segmentation(&data);
      image_sig_detect_textured(&data);
    }
  image_sig_finish(&data, sig);
  image_sig_cleanup(&data);
  return 1;
}
