/*
 *	Image Library -- Image segmentation
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

#ifdef LOCAL_DEBUG
static void
dump_segmentation(struct image_sig_region *regions, uns regions_count)
{
  uns cols = 0, rows = 0;
  for (uns i = 0; i < regions_count; i++)
    for (struct image_sig_block *b = regions[i].blocks; b; b = b->next)
      {
	cols = MAX(cols, b->x);
	rows = MAX(rows, b->y);
      }
  uns size = (cols + 1) * rows;
  byte buf[size];
  bzero(buf, size);
  for (uns i = 0; i < regions_count; i++)
    {
      byte c = (i < 10) ? '0' + i : 'A' - 10 + i;
      for (struct image_sig_block *b = regions[i].blocks; b; b = b->next)
        buf[b->x + b->y * (cols + 1)] = c;
    }
  for (uns i = 0; i < rows; i++)
    log(L_DEBUG, "%s", &buf[i * (cols + 1)]);
}
#endif

/* Pre-quantization - recursively split groups of blocks with large error */

static inline void
prequant_init_region(struct image_sig_region *region)
{
  bzero(region, sizeof(*region));
}

static inline void
prequant_add_block(struct image_sig_region *region, struct image_sig_block *block)
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
prequant_finish_region(struct image_sig_region *region)
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
prequant_heap_cmp(struct image_sig_region *a, struct image_sig_region *b)
{
  return a->e > b->e;
}

#define ASORT_PREFIX(x) prequant_##x
#define ASORT_KEY_TYPE u32
#define ASORT_ELT(i) val[i]
#define ASORT_EXTRA_ARGS , u32 *val
#include "lib/arraysort.h"

static uns
prequant(struct image_sig_block *blocks, uns blocks_count, struct image_sig_region *regions)
{
  DBG("Starting pre-quantization");
  
  uns regions_count, heap_count, axis, cov;
  struct image_sig_block *blocks_end = blocks + blocks_count, *block, *block2;
  struct image_sig_region *heap[IMAGE_REG_MAX + 1], *region, *region2;

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
	  HEAP_DELMIN(struct image_sig_region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP);
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
      HEAP_INCREASE(struct image_sig_region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP, 1);
      heap[++heap_count] = region2;
      HEAP_INSERT(struct image_sig_region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP);
    }

  DBG("Pre-quantized to %u regions", regions_count);

  return regions_count;
}

/* Post-quantization - run a few K-mean iterations to improve pre-quantized regions */

static uns
postquant(struct image_sig_block *blocks, uns blocks_count, struct image_sig_region *regions, uns regions_count)
{
  DBG("Starting post-quantization");
  
  struct image_sig_block *blocks_end = blocks + blocks_count, *block;
  struct image_sig_region *regions_end = regions + regions_count, *region;
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
	  struct image_sig_region *best_region = NULL;
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

uns
image_sig_segmentation(struct image_sig_block *blocks, uns blocks_count, struct image_sig_region *regions)
{
  uns regions_count;
  regions_count = prequant(blocks, blocks_count, regions);
#ifdef LOCAL_DEBUG  
  dump_segmentation(regions, regions_count);
#endif  
  regions_count = postquant(blocks, blocks_count, regions, regions_count);
#ifdef LOCAL_DEBUG  
  dump_segmentation(regions, regions_count);
#endif  
  return regions_count;
}

