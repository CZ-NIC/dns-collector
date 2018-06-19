/*
 *	Image Library -- Image segmentation
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/heap.h>
#include <images/images.h>
#include <images/signature.h>
#include <images/math.h>

#include <string.h>

#ifdef LOCAL_DEBUG
static void
dump_segmentation(struct image_sig_region *regions, uint regions_count)
{
  uint cols = 0, rows = 0;
  for (uint i = 0; i < regions_count; i++)
    for (struct image_sig_block *b = regions[i].blocks; b; b = b->next)
      {
	cols = MAX(cols, b->x + 1);
	rows = MAX(rows, b->y + 1);
      }
  uint size = (cols + 1) * rows;
  byte buf[size];
  bzero(buf, size);
  for (uint i = 0; i < regions_count; i++)
    {
      byte c = (i < 10) ? '0' + i : 'A' - 10 + i;
      for (struct image_sig_block *b = regions[i].blocks; b; b = b->next)
        buf[b->x + b->y * (cols + 1)] = c;
    }
  for (uint i = 0; i < rows; i++)
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
  for (uint i = 0; i < IMAGE_VEC_F; i++)
    {
      region->b[i] += block->v[i];
      region->c[i] += isqr(block->v[i]);
    }
}

static void
prequant_finish_region(struct image_sig_region *region)
{
  if (region->count < 2)
    {
      region->e = 0;
    }
  else
    {
      u64 a = 0;
      region->e = 0;
      for (uint i = 0; i < IMAGE_VEC_F; i++)
        {
	  region->e += region->c[i];
	  a += (u64)region->b[i] * region->b[i];
	}
      region->e -= a / region->count;
      DBG("Finished region %u", (uint)region->e / region->count);
    }
}

static inline uint
prequant_heap_cmp(struct image_sig_region *a, struct image_sig_region *b)
{
  return a->e > b->e;
}

#define ASORT_PREFIX(x) prequant_##x
#define ASORT_KEY_TYPE uint
#include <ucw/sorter/array-simple.h>

static uint
prequant(struct image_sig_block *blocks, uint blocks_count, struct image_sig_region *regions)
{
  DBG("Starting pre-quantization");

  uint regions_count, heap_count, axis;
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
	  regions_count, heap_count, region->count, (uint)region->e);
      if (region->count < 2 ||
	  region->e < image_sig_prequant_thresholds[regions_count - 1] * blocks_count)
        {
	  HEAP_DELETE_MIN(struct image_sig_region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP);
	  continue;
	}

      /* Select axis to split - the one with maximum average quadratic error */
      axis = 0;
      u64 cov = (u64)region->count * region->c[0] - (u64)region->b[0] * region->b[0];
      for (uint i = 1; i < 6; i++)
        {
	  uint j = (u64)region->count * region->c[i] - (u64)region->b[i] * region->b[i];
	  if (j > cov)
	    {
	      axis = i;
	      cov = j;
	    }
	}
      DBG("Splitting axis %u with average quadratic error %u", axis, (uint)(cov / (region->count * region->count)));

      /* Sort values on the split axis */
      uint val[256], cnt[256], cval;
      if (region->count > 64)
        {
	  bzero(cnt, sizeof(cnt));
	  for (block = region->blocks; block; block = block->next)
	    cnt[block->v[axis]]++;
	  cval = 0;
	  for (uint i = 0; i < 256; i++)
	    if (cnt[i])
	      {
		val[cval] = i;
		cnt[cval] = cnt[i];
		cval++;
	      }
	}
      else
        {
          block = region->blocks;
          for (uint i = 0; i < region->count; i++, block = block->next)
	    val[i] = block->v[axis];
	  prequant_sort(val, region->count);
	  cval = 1;
	  cnt[0] = 1;
	  for (uint i = 1; i < region->count; i++)
	    if (val[i] == val[cval - 1])
	      cnt[cval - 1]++;
	    else
	      {
		val[cval] = val[i];
		cnt[cval] = 1;
		cval++;
	      }
	}

      /* Select split value - to minimize error */
      uint b1 = val[0] * cnt[0];
      uint c1 = isqr(val[0]) * cnt[0];
      uint b2 = region->b[axis] - b1;
      uint c2 = region->c[axis] - c1;
      uint i = cnt[0], j = region->count - cnt[0];
      u64 best_err = c1 - (u64)b1 * b1 / i + c2 - (u64)b2 * b2 / j;
      uint split_val = val[0];
      for (uint k = 1; k < cval - 1; k++)
        {
	  uint b0 = val[k] * cnt[k];
	  uint c0 = isqr(val[k]) * cnt[k];
	  b1 += b0;
	  b2 -= b0;
	  c1 += c0;
	  c2 -= c0;
	  i += cnt[k];
	  j -= cnt[k];
	  u64 err = (u64)c1 - (u64)b1 * b1 / i + (u64)c2 - (u64)b2 * b2 / j;
	  if (err < best_err)
	    {
	      best_err = err;
	      split_val = val[k];
	    }
	}
      DBG("split_val=%u best_err=%llu b[axis]=%u c[axis]=%u", split_val, (long long)best_err, region->b[axis], region->c[axis]);

      /* Split region */
      block = region->blocks;
      region2 = regions + regions_count++;
      prequant_init_region(region);
      prequant_init_region(region2);
      while (block)
        {
	  block2 = block->next;
	  if (block->v[axis] <= split_val)
	    prequant_add_block(region, block);
	  else
	    prequant_add_block(region2, block);
	  block = block2;
	}
      prequant_finish_region(region);
      prequant_finish_region(region2);
      HEAP_INCREASE(struct image_sig_region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP, 1, region);
      HEAP_INSERT(struct image_sig_region *, heap, heap_count, prequant_heap_cmp, HEAP_SWAP, region2);
    }

  DBG("Pre-quantized to %u regions", regions_count);

  return regions_count;
}

/* Post-quantization - run a few K-mean iterations to improve pre-quantized regions */

static uint
postquant(struct image_sig_block *blocks, uint blocks_count, struct image_sig_region *regions, uint regions_count)
{
  DBG("Starting post-quantization");

  struct image_sig_block *blocks_end = blocks + blocks_count, *block;
  struct image_sig_region *regions_end = regions + regions_count, *region;
  uint error = 0, last_error;

  /* Initialize regions and initial segmentation error */
  for (region = regions; region != regions_end; )
    {
      uint inv = 0xffffffffU / region->count;
      for (uint i = 0; i < IMAGE_VEC_F; i++)
        {
          region->a[i] = ((u64)region->b[i] * inv) >> 32;
          error += region->c[i] - region->a[i] * region->b[i];
        }
      region++;
    }

  /* Convergation cycle */
  for (uint step = 0; step < image_sig_postquant_max_steps; step++)
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
	  uint best_dist = ~0U;
	  for (region = regions; region != regions_end; region++)
	    {
	      uint dist =
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
	  for (uint i = 0; i < IMAGE_VEC_F; i++)
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
	    uint inv = 0xffffffffU / region->count;
	    for (uint i = 0; i < IMAGE_VEC_F; i++)
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
	  if (dif * image_sig_postquant_threshold < (u64)last_error * 100)
	    break;
	}
    }

  DBG("Post-quantized to %u regions with average square error %u", regions_end - regions, error / blocks_count);

  return regions_end - regions;
}

void
image_sig_segmentation(struct image_sig_data *data)
{
  data->regions_count = prequant(data->blocks, data->blocks_count, data->regions);
#ifdef LOCAL_DEBUG
  dump_segmentation(data->regions, data->regions_count);
#endif
  data->regions_count = postquant(data->blocks, data->blocks_count, data->regions, data->regions_count);
#ifdef LOCAL_DEBUG
  dump_segmentation(data->regions, data->regions_count);
#endif
}

