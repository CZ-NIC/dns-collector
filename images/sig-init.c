/*
 *	Image Library -- Computation of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/math.h"
#include "lib/fastbuf.h"
#include "lib/conf.h"
#include "lib/heap.h"
#include "images/math.h"
#include "images/images.h"
#include "images/color.h"
#include "images/signature.h"

#include <alloca.h>

static double image_sig_inertia_scale[3] = { 3, 1, 0.3 };

struct block {
  u32 area;		/* block area in pixels (usually 16) */
  u32 v[IMAGE_VEC_F];
  u32 x, y;		/* block position */
  struct block *next;
};

struct region {
  struct block *blocks;
  u32 count;
  u32 a[IMAGE_VEC_F];
  u32 b[IMAGE_VEC_F];
  u32 c[IMAGE_VEC_F];
  u64 e;
  u64 w_sum;
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

/* Pre-quantization - recursively split groups of blocks with large error */

static inline void
prequant_init_region(struct region *region)
{
  bzero(region, sizeof(*region));
}

static inline void
prequant_add_block(struct region *region, struct block *block)
{
  block->next = region->blocks;
  region->blocks = block;
  region->count++;
  for (uns i = 0; i < IMAGE_VEC_F; i++)
    {
      region->b[i] += block->v[i];
      region->c[i] += isqr(block->v[i]);
    }
}

static void
prequant_finish_region(struct region *region)
{
  if (region->count < 2)
    memcpy(region->c, region->a, sizeof(region->c));
  else
    {
      u64 a = 0;
      region->e = 0;
      for (uns i = 0; i < IMAGE_VEC_F; i++)
        {
	  region->e += region->c[i];
	  a += (u64)region->b[i] * region->b[i];
	}
      region->e -= a / region->count;
    }
}

static inline uns
prequant_heap_cmp(struct region *a, struct region *b)
{
  return a->e > b->e;
}

#define ASORT_PREFIX(x) prequant_##x
#define ASORT_KEY_TYPE u32
#define ASORT_ELT(i) val[i]
#define ASORT_EXTRA_ARGS , u32 *val
#include "lib/arraysort.h"

static uns
prequant(struct block *blocks, uns blocks_count, struct region *regions)
{
  DBG("Starting pre-quantization");
  
  uns regions_count, heap_count, axis, cov;
  struct block *blocks_end = blocks + blocks_count, *block, *block2;
  struct region *heap[IMAGE_REG_MAX + 1], *region, *region2;

  /* Initialize single region with all blocks */
  regions_count = heap_count = 1;
  heap[1] = regions;
  prequant_init_region(regions);
  for (block = blocks; block != blocks_end; block++)
    prequant_add_block(regions, block);
  prequant_finish_region(regions);

  /* Main cycle */
  while (regions_count < IMAGE_REG_MAX &&
      regions_count <= DARY_LEN(image_sig_prequant_thresholds) && heap_count)
    {
      region = heap[1];
      DBG("Step... regions_count=%u heap_count=%u region->count=%u, region->e=%u", 
	  regions_count, heap_count, region->count, (uns)region->e);
      if (region->count < 2 ||
	  region->e < image_sig_prequant_thresholds[regions_count - 1] * region->count)
        {
	  HEAP_DELMIN(struct region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP);
	  continue;
	}

      /* Select axis to split - the one with maximum covariance */
      axis = 0;
      cov = region->count * region->c[0] - region->b[0];
      for (uns i = 1; i < 6; i++)
        {
	  uns j = region->count * region->c[i] - region->b[i];
	  if (j > cov)
	    {
	      axis = i;
	      cov = j;
	    }
	}
      DBG("Splitting axis %u with covariance %u", axis, cov / region->count);

      /* Sort values on the split axis */
      u32 val[region->count];
      block = region->blocks;
      for (uns i = 0; i < region->count; i++, block = block->next)
	val[i] = block->v[axis];
      prequant_sort(region->count, val);

      /* Select split value - to minimize error */
      uns b1 = 0, c1 = 0, b2 = region->b[axis], c2 = region->c[axis];
      uns i = 0, j = region->count, best_v = 0;
      u64 best_err = 0xffffffffffffffff;
      while (i < region->count)
        {
	  u64 err = (u64)i * c1 - (u64)b1 * b1 + (u64)j * c2 - (u64)b2 * b2;
	  if (err < best_err)
	    {
	      best_err = err;
	      best_v = val[i];
	    }
	  uns sqr = isqr(val[i]);
	  b1 += val[i];
	  b2 -= val[i];
	  c1 += sqr;
	  c2 -= sqr;
	  i++;
	  j--;
	}
      uns split_val = best_v;
      DBG("split_val=%u best_err=%Lu b[axis]=%u c[axis]=%u", split_val, (long long)best_err, region->b[axis], region->c[axis]);

      /* Split region */
      block = region->blocks;
      region2 = regions + regions_count++;
      prequant_init_region(region);
      prequant_init_region(region2);
      while (block)
        {
	  block2 = block->next;
	  if (block->v[axis] < split_val)
	    prequant_add_block(region, block);
	  else
	    prequant_add_block(region2, block);
	  block = block2;
	}
      prequant_finish_region(region);
      prequant_finish_region(region2);
      HEAP_INCREASE(struct region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP, 1);
      heap[++heap_count] = region2;
      HEAP_INSERT(struct region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP);
    }

  DBG("Pre-quantized to %u regions", regions_count);

  return regions_count;
}

/* Post-quantization - run a few K-mean iterations to improve pre-quantized regions */

static uns
postquant(struct block *blocks, uns blocks_count, struct region *regions, uns regions_count)
{
  DBG("Starting post-quantization");
  
  struct block *blocks_end = blocks + blocks_count, *block;
  struct region *regions_end = regions + regions_count, *region;
  uns error = 0, last_error;

  /* Initialize regions and initial segmentation error */
  for (region = regions; region != regions_end; )
    {
      uns inv = 0xffffffffU / region->count;
      for (uns i = 0; i < IMAGE_VEC_F; i++)
        {
          region->a[i] = ((u64)region->b[i] * inv) >> 32;
          error += region->c[i] - region->a[i] * region->b[i];
        }
      region++;
    }

  /* Convergation cycle */
  for (uns step = 0; step < image_sig_postquant_max_steps; step++)
    {
      DBG("Step...");
      
      /* Clear regions */
      for (region = regions; region != regions_end; region++)
        {
	  region->blocks = NULL;
	  region->count = 0;
	  bzero(region->b, sizeof(region->b));
	  bzero(region->c, sizeof(region->c));
	}

      /* Assign each block to its nearest pivot and accumulate region variables */
      for (block = blocks; block != blocks_end; block++)
        {
	  struct region *best_region = NULL;
	  uns best_dist = ~0U;
	  for (region = regions; region != regions_end; region++)
	    {
	      uns dist =
		isqr(block->v[0] - region->a[0]) +
		isqr(block->v[1] - region->a[1]) +
		isqr(block->v[2] - region->a[2]) +
		isqr(block->v[3] - region->a[3]) +
		isqr(block->v[4] - region->a[4]) +
		isqr(block->v[5] - region->a[5]);
	      if (dist <= best_dist)
	        {
		  best_dist = dist;
		  best_region = region;
		}
	    }
	  region = best_region;
	  region->count++;
	  block->next = region->blocks;
	  region->blocks = block;
	  for (uns i = 0; i < IMAGE_VEC_F; i++)
	    {
	      region->b[i] += block->v[i];
	      region->c[i] += isqr(block->v[i]);
	    }
	}

      /* Finish regions, delete empty ones (should appear rarely), compute segmentation error */
      last_error = error;
      error = 0;
      for (region = regions; region != regions_end; )
	if (region->count)
          {
	    uns inv = 0xffffffffU / region->count;
	    for (uns i = 0; i < IMAGE_VEC_F; i++)
	      {
	        region->a[i] = ((u64)region->b[i] * inv) >> 32;
	        error += region->c[i] - region->a[i] * region->b[i];
	      }
	    region++;
	  }
        else
	  {
	    regions_end--;
	    *region = *regions_end;
	  }

      DBG("last_error=%u error=%u", last_error, error);

      /* Convergation criteria */
      if (step >= image_sig_postquant_min_steps)
        {
	  if (error > last_error)
	    break;
	  u64 dif = last_error - error;
	  if (dif * image_sig_postquant_threshold < last_error * 100)
	    break;
	}
    }

  DBG("Post-quantized to %u regions with average square error %u", regions_end - regions, error / blocks_count);
  
  return regions_end - regions;
}

static inline uns
segmentation(struct block *blocks, uns blocks_count, struct region *regions, uns width UNUSED, uns height UNUSED)
{
  uns regions_count;
  regions_count = prequant(blocks, blocks_count, regions);
#ifdef LOCAL_DEBUG  
  dump_segmentation(regions, regions_count, width, height);
#endif  
  regions_count = postquant(blocks, blocks_count, regions, regions_count);
#ifdef LOCAL_DEBUG  
  dump_segmentation(regions, regions_count, width, height);
#endif  
  return regions_count;
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
	      block->v[0] = (l_sum >> 4);
	      block->v[1] = (u_sum >> 4);
	      block->v[2] = (v_sum >> 4);
	    }
	  /* Incomplete square near the edge */
	  else
	    {
	      uns x, y;
	      uns square_cols = (block_x < w - 1 || !(cols & 3)) ? 4 : cols & 3;
	      uns square_rows = (block_y < h - 1 || !(rows & 3)) ? 4 : rows & 3;
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
	      block->v[0] = (l_sum * div) >> 16;
	      block->v[1] = (u_sum * div) >> 16;
	      block->v[2] = (v_sum * div) >> 16;
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
	      t[i + 8] = (DAUB_3 * s[i + 8] - DAUB_2 * s[i +12] + DAUB_1 * s[i + 0] - DAUB_0 * s[i + 4]) / 0x2000;
	      t[i +12] = (DAUB_3 * s[i + 0] - DAUB_2 * s[i + 4] + DAUB_1 * s[i + 8] - DAUB_0 * s[i +12]) / 0x2000;
	    }
	  for (; i < 4; i++)
	    {
	      t[i + 0] = (DAUB_0 * s[i + 8] + DAUB_1 * s[i +12] + DAUB_2 * s[i + 0] + DAUB_3 * s[i + 4]) / 0x2000;
	      t[i + 4] = (DAUB_0 * s[i + 0] + DAUB_1 * s[i + 4] + DAUB_2 * s[i + 8] + DAUB_3 * s[i +12]) / 0x2000;
	      t[i + 8] = (DAUB_3 * s[i + 8] - DAUB_2 * s[i +12] + DAUB_1 * s[i + 0] - DAUB_0 * s[i + 4]) / 0x2000;
	      t[i +12] = (DAUB_3 * s[i + 0] - DAUB_2 * s[i + 4] + DAUB_1 * s[i + 8] - DAUB_0 * s[i +12]) / 0x2000;
	    }

	  /* Extract energies in LH, HL and HH bands */
	  block->v[3] = fast_sqrt_u16(isqr(t[8]) + isqr(t[9]) + isqr(t[12]) + isqr(t[13]));
	  block->v[4] = fast_sqrt_u16(isqr(t[2]) + isqr(t[3]) + isqr(t[6]) + isqr(t[7]));
	  block->v[5] = fast_sqrt_u16(isqr(t[10]) + isqr(t[11]) + isqr(t[14]) + isqr(t[15]));
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
      l_sum += blocks[i].v[0];
      u_sum += blocks[i].v[1];
      v_sum += blocks[i].v[2];
      lh_sum += blocks[i].v[3];
      hl_sum += blocks[i].v[4];
      hh_sum += blocks[i].v[5];
    }

  sig->vec.f[0] = l_sum / blocks_count;
  sig->vec.f[1] = u_sum / blocks_count;
  sig->vec.f[2] = v_sum / blocks_count;
  sig->vec.f[3] = lh_sum / blocks_count;
  sig->vec.f[4] = hl_sum / blocks_count;
  sig->vec.f[5] = hh_sum / blocks_count;

  if (cols < image_sig_min_width || rows < image_sig_min_height)
    {
      xfree(blocks);
      return 1;
    }

  /* Quantize blocks to image regions */
  struct region regions[IMAGE_REG_MAX];
  sig->len = segmentation(blocks, blocks_count, regions, w, h);

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
      sig->reg[i].f[0] = r->a[0];
      sig->reg[i].f[1] = r->a[1];
      sig->reg[i].f[2] = r->a[2];
      sig->reg[i].f[3] = r->a[3];
      sig->reg[i].f[4] = r->a[4];
      sig->reg[i].f[5] = r->a[5];

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
#endif

  xfree(blocks);

  return 1;
}

