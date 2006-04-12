#define LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/math.h"
#include "lib/fastbuf.h"
#include "images/images.h"

#include <magick/api.h>

/*
 * Color spaces
 * 
 * http://www.tecgraf.puc-rio.br/~mgattass/color/ColorIndex.html
 * 
 */

#define REF_WHITE_X 0.96422
#define REF_WHITE_Y 1.
#define REF_WHITE_Z 0.82521

/* sRGB to XYZ */
static void
srgb_to_xyz_slow(double srgb[3], double xyz[3])
{
  double a[3];
  for (uns i = 0; i < 3; i++)
    if (srgb[i] > 0.04045)
      a[i] = pow((srgb[i] + 0.055) * (1 / 1.055), 2.4);
    else
      a[i] = srgb[i] * (1 / 12.92);
  xyz[0] = 0.412424 * a[0] + 0.357579 * a[1] + 0.180464 * a[2];
  xyz[1] = 0.212656 * a[0] + 0.715158 * a[1] + 0.072186 * a[2];
  xyz[2] = 0.019332 * a[0] + 0.119193 * a[1] + 0.950444 * a[2];
}

/* XYZ to CIE-Luv */
static void
xyz_to_luv_slow(double xyz[3], double luv[3])
{
  double sum = xyz[0] + 15 * xyz[1] + 3 * xyz[2];
  if (sum < 0.000001)
    luv[0] = luv[1] = luv[2] = 0;
  else
    {
      double var_u = 4 * xyz[0] / sum;
      double var_v = 9 * xyz[1] / sum;
      if (xyz[1] > 0.008856)
        luv[0] = 116 * pow(xyz[1], 1 / 3.) - 16;
      else
        luv[0] = (116 * 7.787) * xyz[1];
      luv[1] = luv[0] * (13 * (var_u - 4 * REF_WHITE_X / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z)));
      luv[2] = luv[0] * (13 * (var_v - 9 * REF_WHITE_Y / (REF_WHITE_X + 15 * REF_WHITE_Y + 3 * REF_WHITE_Z)));
      /* intervals [0..100], [-134..220], [-140..122] */
    }
}

struct block {
  uns l, u, v;		/* average Luv coefficients */
  uns lh, hl, hh;	/* energies in Daubechies wavelet bands */
};

static void
compute_image_area_signature(PixelPacket *pixels, uns width, uns height, struct image_signature *sig)
{
  ASSERT(width >= 4 && height >= 4);

  uns w = width >> 2;
  uns h = height >> 2;
  DBG("Computing signature for image %dx%d... %dx%d blocks", width, height, w, h);
  uns blocks_count = w * h;
  struct block *blocks = xmalloc(blocks_count * sizeof(struct block)), *block = blocks; /* FIXME: use mempool */
  
  /* Every 4x4 block (FIXME: deal with smaller blocks near the edges) */
  PixelPacket *p = pixels;
  for (uns block_y = 0; block_y < h; block_y++, p += (width & 3) + width * 3)
    for (uns block_x = 0; block_x < w; block_x++, p -= 4 * width - 4, block++)
      {
        int t[16], s[16], *tp = t;

	/* Convert pixels to Luv color space and compute average coefficients 
	 * FIXME:
	 * - could be MUCH faster with precomputed tables and integer arithmetic... 
	 *   I will propably use interpolation in 3-dim array */
	uns l_sum = 0;
	uns u_sum = 0;
	uns v_sum = 0;
	for (uns y = 0; y < 4; y++, p += width - 4)
	  for (uns x = 0; x < 4; x++, p++)
	    {
	      double rgb[3], luv[3], xyz[3];
	      rgb[0] = (p->red >> (QuantumDepth - 8)) / 255.;
	      rgb[1] = (p->green >> (QuantumDepth - 8)) / 255.;
	      rgb[2] = (p->blue >> (QuantumDepth - 8)) / 255.;
	      srgb_to_xyz_slow(rgb, xyz);
	      xyz_to_luv_slow(xyz, luv);
	      l_sum += *tp++ = luv[0];
	      u_sum += luv[1] + 150;
	      v_sum += luv[2] + 150;
	    }

	block->l = l_sum;
	block->u = u_sum;
	block->v = v_sum;

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
	block->lh = sqrt(t[8] * t[8] + t[9] * t[9] + t[12] * t[12] + t[13] * t[13]);
	block->hl = sqrt(t[2] * t[2] + t[3] * t[3] + t[6] * t[6] + t[7] * t[7]);
	block->hh = sqrt(t[10] * t[10] + t[11] * t[11] + t[14] * t[14] + t[15] * t[15]);
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
}

static ExceptionInfo exception;
static QuantizeInfo quantize_info;
static ImageInfo *image_info;

void
compute_image_signature_prepare(void)
{
  InitializeMagick(NULL); 
  GetExceptionInfo(&exception);
  image_info = CloneImageInfo(NULL);
  image_info->subrange = 1;
  GetQuantizeInfo(&quantize_info);
  quantize_info.colorspace = RGBColorspace;
}

void
compute_image_signature_finish(void)
{
  DestroyImageInfo(image_info);
  DestroyExceptionInfo(&exception);
  DestroyMagick();
}

int
compute_image_signature(void *data, uns len, struct image_signature *sig)
{
  Image *image = BlobToImage(image_info, data, len, &exception); /* Damn slow... takes most of CPU time :-/ */
  if (!image)
    die("Invalid image format");
  if (image->columns < 4 || image->rows < 4)
    {
      DBG("Image too small (%dx%d)", (int)image->columns, (int)image->rows);
      DestroyImage(image);
      return -1;
    }
  QuantizeImage(&quantize_info, image); /* Also slow... and propably not necessary... */
  PixelPacket *pixels = (PixelPacket *) AcquireImagePixels(image, 0, 0, image->columns, image->rows, &exception);
  compute_image_area_signature(pixels, image->columns, image->rows, sig);
  DestroyImage(image);
  return 0;
}

