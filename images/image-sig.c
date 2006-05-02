#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/math.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "images/image-obj.h"
#include "images/image-sig.h"
#include "images/color.h"

#include <alloca.h>

struct block {
  uns l, u, v;		/* average Luv coefficients */
  uns lh, hl, hh;	/* energies in Daubechies wavelet bands */
};

int
compute_image_signature(struct image *image, struct image_signature *sig)
{
  uns width = image->width;
  uns height = image->height;

  if (width < 4 || height < 4)
    {
      DBG("Image too small... %dx%d", width, height);
      return 0;
    }
  
  uns w = width >> 2;
  uns h = height >> 2;
  DBG("Computing signature for image %dx%d... %dx%d blocks", width, height, w, h);
  uns blocks_count = w * h;
  struct block *blocks = xmalloc(blocks_count * sizeof(struct block)), *block = blocks; /* FIXME: use mempool */
  
  /* Every 4x4 block (FIXME: deal with smaller blocks near the edges) */
  byte *p = image->pixels;
  for (uns block_y = 0; block_y < h; block_y++, p += 3 * ((width & 3) + width * 3))
    for (uns block_x = 0; block_x < w; block_x++, p -= 3 * (4 * width - 4), block++)
      {
        int t[16], s[16], *tp = t;

	/* Convert pixels to Luv color space and compute average coefficients 
	 * FIXME:
	 * - could be MUCH faster with precomputed tables and integer arithmetic... 
	 *   I will propably use interpolation in 3-dim array */
	uns l_sum = 0;
	uns u_sum = 0;
	uns v_sum = 0;
	for (uns y = 0; y < 4; y++, p += 3 * (width - 4))
	  for (uns x = 0; x < 4; x++, p += 3)
	    {
	      byte luv[3];
	      srgb_to_luv_pixel(luv, p);
	      l_sum += *tp++ = luv[0];
	      u_sum += luv[1];
	      v_sum += luv[2];
	    }

	block->l = (l_sum >> 4);
	block->u = (u_sum >> 4);
	block->v = (v_sum >> 4);

	/* Apply Daubechies wavelet transformation 
	 * FIXME:
	 * - MMX/SSE instructions or tables could be faster 
	 * - maybe it would be better to compute Luv and wavelet separately because of processor cache or MMX/SSE 
	 * - eliminate slow square roots 
	 * - what about Haar transformation? */

#define DAUB_0	31651 /* (1 + sqrt 3) / (4 * sqrt 2) */
#define DAUB_1	54822 /* (3 + sqrt 3) / (4 * sqrt 2) */
#define DAUB_2	14689 /* (3 - sqrt 3) / (4 * sqrt 2) */
#define DAUB_3	-8481 /* (1 - sqrt 3) / (4 * sqrt 2) */

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

  sig->len = 0;

  xfree(blocks);

  DBG("Resulting signature is (%s)", stk_print_image_vector(&sig->vec));
  return 1;
}

