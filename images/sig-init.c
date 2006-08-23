/*
 *	Image Library -- Computation of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/math.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "images/color.h"
#include "images/signature.h"
#include <alloca.h>

static double image_sig_inertia_scale[3] = { 3, 1, 0.3 };

struct block {
  u32 area;		/* block area in pixels (usually 16) */
  u32 l, u, v;		/* average Luv coefficients */
  u32 lh, hl, hh;	/* energies in Daubechies wavelet bands */
  u32 x, y;		/* block position */
  struct block *next;
};

struct region {
  u32 l, u, v;
  u32 lh, hl, hh;
  u32 sum_l, sum_u, sum_v;
  u32 sum_lh, sum_hl, sum_hh;
  u32 count;
  u64 w_sum;
  struct block *blocks;
};

static inline uns
dist(uns a, uns b)
{
  int d = a - b;
  return d * d;
}

#ifdef LOCAL_DEBUG
static void
dump_segmentation(struct region *regions, uns regions_count, uns cols, uns rows)
{
  uns size = (cols + 1) * rows;
  byte buf[size];
  bzero(buf, size);
  for (uns i = 0; i < regions_count; i++)
    {
      byte c = (i < 10) ? '0' + i : 'A' - 10 + i;
      for (struct block *b = regions[i].blocks; b; b = b->next)
        buf[b->x + b->y * (cols + 1)] = c;
    }
  for (uns i = 0; i < rows; i++)
    log(L_DEBUG, "%s", &buf[i * (cols + 1)]);
}
#endif

/* FIXME: SLOW! */
static uns
compute_k_means(struct block *blocks, uns blocks_count, struct region *regions, uns regions_count)
{
  ASSERT(regions_count <= blocks_count);
  struct block *mean[IMAGE_REG_MAX], *b, *blocks_end = blocks + blocks_count;
  struct region *r, *regions_end = regions + regions_count;

  /* Select means_count random blocks as initial regions pivots */
  if (regions_count <= blocks_count - regions_count)
    {
      for (b = blocks; b != blocks_end; b++)
	b->next = NULL;
      for (uns i = 0; i < regions_count; )
        {
          uns j = random_max(blocks_count);
	  b = blocks + j;
	  if (!b->next)
	    b->next = mean[i++] = b;
        }
    }
  else
    {
      uns j = blocks_count;
      for (uns i = regions_count; i; j--)
	if (random_max(j) <= i)
	  mean[--i] = blocks + j - 1;
    }
  r = regions;
  for (uns i = 0; i < regions_count; i++, r++)
    {
      b = mean[i];
      r->l = b->l;
      r->u = b->u;
      r->v = b->v;
      r->lh = b->lh;
      r->hl = b->hl;
      r->hh = b->hh;
    }

  /* Convergation cycle */
  for (uns conv_i = 8; ; conv_i--)
    {
      for (r = regions; r != regions_end; r++)
        {
          r->sum_l = r->sum_u = r->sum_v = r->sum_lh = r->sum_hl = r->sum_hh = r->count = 0;
	  r->blocks = NULL;
	}

      /* Find nearest regions and accumulate averages */
      for (b = blocks; b != blocks_end; b++)
        {
	  uns best_d = ~0U;
	  struct region *best_r = NULL;
	  for (r = regions; r != regions_end; r++)
	    {
	      uns d =
		dist(r->l, b->l) +
		dist(r->u, b->u) +
		dist(r->v, b->v) +
		dist(r->lh, b->lh) +
		dist(r->hl, b->hl) +
		dist(r->hh, b->hh);
	      if (d < best_d)
	        {
		  best_d = d;
		  best_r = r;
		}
	    }
	  best_r->sum_l += b->l;
	  best_r->sum_u += b->u;
	  best_r->sum_v += b->v;
	  best_r->sum_lh += b->lh;
	  best_r->sum_hl += b->hl;
	  best_r->sum_hh += b->hh;
	  best_r->count++;
	  b->next = best_r->blocks;
	  best_r->blocks = b;
	}

      /* Compute new averages */
      for (r = regions; r != regions_end; r++)
	if (r->count)
          {
	    r->l = r->sum_l / r->count;
	    r->u = r->sum_u / r->count;
	    r->v = r->sum_v / r->count;
	    r->lh = r->sum_lh / r->count;
	    r->hl = r->sum_hl / r->count;
	    r->hh = r->sum_hh / r->count;
	  }

      if (!conv_i)
	break; // FIXME: convergation criteria
    }

  /* Remove empty regions */
  struct region *r2 = regions;
  for (r = regions; r != regions_end; r++)
    if (r->count)
      *r2++ = *r;
  return r2 - regions;
}

int
compute_image_signature(struct image_thread *thread UNUSED, struct image_signature *sig, struct image *image)
{
  bzero(sig, sizeof(*sig));
  ASSERT((image->flags & IMAGE_PIXEL_FORMAT) == COLOR_SPACE_RGB);
  uns cols = image->cols;
  uns rows = image->rows;
  uns row_size = image->row_size;

  uns w = (cols + 3) >> 2;
  uns h = (rows + 3) >> 2;

  DBG("Computing signature for image of %ux%u pixels (%ux%u blocks)", cols, rows, w, h);

  uns blocks_count = w * h;
  struct block *blocks = xmalloc(blocks_count * sizeof(struct block)), *block = blocks;

  /* Every block of 4x4 pixels */
  byte *row_start = image->pixels;
  for (uns block_y = 0; block_y < h; block_y++, row_start += row_size * 4)
    {
      byte *p = row_start;
      for (uns block_x = 0; block_x < w; block_x++, p += 12, block++)
        {
          int t[16], s[16], *tp = t;
	  block->x = block_x;
	  block->y = block_y;

	  /* Convert pixels to Luv color space and compute average coefficients */
	  uns l_sum = 0;
	  uns u_sum = 0;
	  uns v_sum = 0;
	  byte *p2 = p;
	  if ((!(cols & 3) || block_x < w - 1) && (!(rows & 3) || block_y < h - 1))
	    {
	      for (uns y = 0; y < 4; y++, p2 += row_size - 12)
	        for (uns x = 0; x < 4; x++, p2 += 3)
	          {
	            byte luv[3];
	            srgb_to_luv_pixel(luv, p2);
	            l_sum += *tp++ = luv[0];
	            u_sum += luv[1];
	            v_sum += luv[2];
	          }
	      block->area = 16;
	      block->l = (l_sum >> 4);
	      block->u = (u_sum >> 4);
	      block->v = (v_sum >> 4);
	    }
	  /* Incomplete square near the edge */
	  else
	    {
	      uns x, y;
	      uns square_cols = (block_x < w - 1) ? 4 : cols & 3;
	      uns square_rows = (block_y < h - 1) ? 4 : rows & 3;
	      for (y = 0; y < square_rows; y++, p2 += row_size)
	        {
		  byte *p3 = p2;
	          for (x = 0; x < square_cols; x++, p3 += 3)
	            {
	              byte luv[3];
	              srgb_to_luv_pixel(luv, p3);
	              l_sum += *tp++ = luv[0];
	              u_sum += luv[1];
	              v_sum += luv[2];
		    }
		  for (; x < 4; x++)
		    {
		      *tp = tp[-square_cols];
		      tp++;
		    }
	        }
	      for (; y < 4; y++)
	        for (x = 0; x < 4; x++)
	          {
	            *tp = tp[-square_rows * 4];
		    tp++;
		  }
	      block->area = square_cols * square_rows;
	      uns div = 0x10000 / block->area;
	      block->l = (l_sum * div) >> 16;
	      block->u = (u_sum * div) >> 16;
	      block->v = (v_sum * div) >> 16;
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
	      t[i + 8] = (DAUB_3 * s[i + 8] - DAUB_2 * s[i +12] + DAUB_1 * s[i + 0] - DAUB_0 * s[i + 4]) / 0x1000;
	      t[i +12] = (DAUB_3 * s[i + 0] - DAUB_2 * s[i + 4] + DAUB_1 * s[i + 8] - DAUB_0 * s[i +12]) / 0x1000;
	    }
	  for (; i < 4; i++)
	    {
	      t[i + 0] = (DAUB_0 * s[i + 8] + DAUB_1 * s[i +12] + DAUB_2 * s[i + 0] + DAUB_3 * s[i + 4]) / 0x1000;
	      t[i + 4] = (DAUB_0 * s[i + 0] + DAUB_1 * s[i + 4] + DAUB_2 * s[i + 8] + DAUB_3 * s[i +12]) / 0x1000;
	      t[i + 8] = (DAUB_3 * s[i + 8] - DAUB_2 * s[i +12] + DAUB_1 * s[i + 0] - DAUB_0 * s[i + 4]) / 0x1000;
	      t[i +12] = (DAUB_3 * s[i + 0] - DAUB_2 * s[i + 4] + DAUB_1 * s[i + 8] - DAUB_0 * s[i +12]) / 0x1000;
	    }

	  /* Extract energies in LH, HL and HH bands */
	  block->lh = CLAMP((int)(sqrt(t[8] * t[8] + t[9] * t[9] + t[12] * t[12] + t[13] * t[13]) / 16), 0, 255);
	  block->hl = CLAMP((int)(sqrt(t[2] * t[2] + t[3] * t[3] + t[6] * t[6] + t[7] * t[7]) / 16), 0, 255);
	  block->hh = CLAMP((int)(sqrt(t[10] * t[10] + t[11] * t[11] + t[14] * t[14] + t[15] * t[15]) / 16), 0, 255);
        }
    }

  /* FIXME: simple average is for testing pusposes only */
  uns l_sum = 0;
  uns u_sum = 0;
  uns v_sum = 0;
  uns lh_sum = 0;
  uns hl_sum = 0;
  uns hh_sum = 0;
  for (uns i = 0; i < blocks_count; i++)
    {
      l_sum += blocks[i].l;
      u_sum += blocks[i].u;
      v_sum += blocks[i].v;
      lh_sum += blocks[i].lh;
      hl_sum += blocks[i].hl;
      hh_sum += blocks[i].hh;
    }

  sig->vec.f[0] = l_sum / blocks_count;
  sig->vec.f[1] = u_sum / blocks_count;
  sig->vec.f[2] = v_sum / blocks_count;
  sig->vec.f[3] = lh_sum / blocks_count;
  sig->vec.f[4] = hl_sum / blocks_count;
  sig->vec.f[5] = hh_sum / blocks_count;

  if (cols < image_sig_min_width || rows < image_sig_min_height)
    return 1;

  /* Quantize blocks to image regions */
  struct region regions[IMAGE_REG_MAX];
  sig->len = compute_k_means(blocks, blocks_count, regions, MIN(blocks_count, IMAGE_REG_MAX));

  /* For each region */
  u64 w_total = 0;
  uns w_border = (MIN(w, h) + 3) / 4;
  uns w_mul = 127 * 256 / w_border;
  for (uns i = 0; i < sig->len; i++)
    {
      struct region *r = regions + i;
      DBG("Processing region %u: count=%u", i, r->count);
      ASSERT(r->count);

      /* Copy texture properties */
      sig->reg[i].f[0] = r->l;
      sig->reg[i].f[1] = r->u;
      sig->reg[i].f[2] = r->v;
      sig->reg[i].f[3] = r->lh;
      sig->reg[i].f[4] = r->hl;
      sig->reg[i].f[5] = r->hh;

      /* Compute coordinates centroid and region weight */
      u64 x_avg = 0, y_avg = 0, w_sum = 0;
      for (struct block *b = r->blocks; b; b = b->next)
        {
	  x_avg += b->x;
	  y_avg += b->y;
	  uns d = b->x;
	  d = MIN(d, b->y);
	  d = MIN(d, w - b->x - 1);
	  d = MIN(d, h - b->y - 1);
	  if (d >= w_border)
	    w_sum += 128;
	  else
	    w_sum += 128 + (d - w_border) * w_mul / 256;
	}
      w_total += w_sum;
      r->w_sum = w_sum;
      x_avg /= r->count;
      y_avg /= r->count;
      DBG("  centroid=(%u %u)", (uns)x_avg, (uns)y_avg);

      /* Compute normalized inertia */
      u64 sum1 = 0, sum2 = 0, sum3 = 0;
      for (struct block *b = r->blocks; b; b = b->next)
        {
	  uns inc2 = dist(x_avg, b->x) + dist(y_avg, b->y);
	  uns inc1 = sqrt(inc2);
	  sum1 += inc1;
	  sum2 += inc2;
	  sum3 += inc1 * inc2;
	}
      sig->reg[i].h[0] = CLAMP(image_sig_inertia_scale[0] * sum1 * ((3 * M_PI * M_PI) / 2) * pow(r->count, -1.5), 0, 65535);
      sig->reg[i].h[1] = CLAMP(image_sig_inertia_scale[1] * sum2 * ((4 * M_PI * M_PI * M_PI) / 2) / ((u64)r->count * r->count), 0, 65535);
      sig->reg[i].h[2] = CLAMP(image_sig_inertia_scale[2] * sum3 * ((5 * M_PI * M_PI * M_PI * M_PI) / 2) * pow(r->count, -2.5), 0, 65535);

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
	      d += dist(sig->reg[i].f[k], sig->reg[j].f[k]);
	    df += sqrt(d);
	    d = 0;
	    for (uns k = 0; k < IMAGE_REG_H; k++)
	      d += dist(sig->reg[i].h[k], sig->reg[j].h[k]);
	    dh += sqrt(d);
	    cnt++;
          }
      sig->df = CLAMP(df / cnt, 1, 255);
      sig->dh = CLAMP(dh / cnt, 1, 65535);
    }
  DBG("Average regions difs: df=%u dh=%u", sig->df, sig->dh);

  /* Compute normalized weights */
  uns wa = 128, wb = 128;
  for (uns i = sig->len; --i > 0; )
    {
      struct region *r = regions + i;
      wa -= sig->reg[i].wa = CLAMP(r->count * 128 / blocks_count, 1, (int)(wa - i));
      wb -= sig->reg[i].wb = CLAMP(r->w_sum * 128 / w_total, 1, (int)(wa - i));
    }
  sig->reg[0].wa = wa;
  sig->reg[0].wb = wb;

  /* Dump regions features */
#ifdef LOCAL_DEBUG
  for (uns i = 0; i < sig->len; i++)
    {
      byte buf[IMAGE_REGION_DUMP_MAX];
      image_region_dump(buf, sig->reg + i);
      DBG("region %u: features=%s", i, buf);
    }
  dump_segmentation(regions, sig->len, w, h);
#endif

  xfree(blocks);

  return 1;
}

