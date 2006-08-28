/*
 *	Image Library -- Comparitions of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#define LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/math.h"
#include "images/math.h"
#include "images/images.h"
#include "images/signature.h"

#include <stdio.h>

static uns
image_signatures_dist_1(struct image_signature *sig1, struct image_signature *sig2)
{
  DBG("image_signatures_dist_1()");

  uns cnt1 = sig1->len;
  uns cnt2 = sig2->len;
  struct image_region *reg1 = sig1->reg;
  struct image_region *reg2 = sig2->reg;
  uns mf[IMAGE_REG_MAX][IMAGE_REG_MAX], mh[IMAGE_REG_MAX][IMAGE_REG_MAX];
  uns lf[IMAGE_REG_MAX * 2], lh[IMAGE_REG_MAX * 2];
  uns df = sig1->df + sig2->df, dh = sig1->dh + sig2->dh;

  /* Compute distance matrix */
  for (uns i = 0; i < cnt1; i++)
    for (uns j = 0; j < cnt2; j++)
      {
	uns d = 0;
	for (uns k = 0; k < IMAGE_REG_F; k++)
	  {
	    int dif = reg1[i].f[k] - reg2[j].f[k];
	    d += dif * dif;
	  }
	mf[i][j] = d;
	d = 0;
	for (uns k = 0; k < IMAGE_REG_H; k++)
	  {
	    int dif = reg1[i].h[k] - reg2[j].h[k];
	    d += dif * dif;
	  }
	mh[i][j] = d;
      }

  uns lfs = 0, lhs = 0;
  for (uns i = 0; i < cnt1; i++)
    {
      uns f = mf[i][0], h = mh[i][0];
      for (uns j = 1; j < cnt2; j++)
        {
	  f = MIN(f, mf[i][j]);
	  h = MIN(h, mh[i][j]);
	}
      lf[i] = (df * 0x10000) / (df + fast_sqrt_u32(f));
      lh[i] = (dh * 0x10000) / (dh + fast_sqrt_u32(h));
      lfs += lf[i] * (6 * reg1[i].wa + 2 * reg1[i].wb);
      lhs += lh[i] * reg1[i].wa;
    }
  for (uns i = 0; i < cnt2; i++)
    {
      uns f = mf[0][i], h = mh[0][i];
      for (uns j = 1; j < cnt1; j++)
        {
	  f = MIN(f, mf[j][i]);
	  h = MIN(h, mh[j][i]);
	}
      lf[i + cnt1] = (df * 0x10000) / (df + fast_sqrt_u32(f));
      lh[i + cnt1] = (dh * 0x10000) / (dh + fast_sqrt_u32(h));
      lfs += lf[i] * (6 * reg2[i].wa + 2 * reg2[i].wb);
      lhs += lh[i] * reg2[i].wa;
    }

  uns measure = lfs * 6 + lhs * 2 * 8;

#ifdef LOCAL_DEBUG
  /* Display similarity vectors */
  byte buf[2 * IMAGE_REG_MAX * 16 + 3], *b = buf;
  for (uns i = 0; i < cnt1 + cnt2; i++)
    {
      if (i)
	*b++ = ' ';
      if (i == cnt1)
	*b++ = '~', *b++ = ' ';
      b += sprintf(b, "%.4f", (double)lf[i] / 0x10000);
    }
  *b = 0;
  DBG("Lf=(%s)", buf);
  b = buf;
  for (uns i = 0; i < cnt1 + cnt2; i++)
    {
      if (i)
	*b++ = ' ';
      if (i == cnt1)
	*b++ = '~', *b++ = ' ';
      b += sprintf(b, "%.4f", (double)lh[i] / 0x10000);
    }
  *b = 0;
  DBG("Lh=(%s)", buf);
  DBG("Lfm=%.4f", lfs / (double)(1 << (3 + 8 + 16)));
  DBG("Lhm=%.4f", lhs / (double)(1 << (8 + 16)));
  DBG("measure=%.4f", measure / (double)(1 << (3 + 3 + 8 + 16)));
#endif

  return (1 << (3 + 3 + 8 + 16)) - measure;
}

#define ASORT_PREFIX(x) image_signatures_dist_2_##x
#define ASORT_KEY_TYPE uns
#define ASORT_ELT(i) items[i]
#define ASORT_EXTRA_ARGS , uns *items
#include "lib/arraysort.h"

static uns
image_signatures_dist_2(struct image_signature *sig1, struct image_signature *sig2)
{
  DBG("image_signatures_dist_2()");

  uns dist[IMAGE_REG_MAX * IMAGE_REG_MAX], p[IMAGE_REG_MAX], q[IMAGE_REG_MAX];
  uns n, i, j, k, l, s, d;
  struct image_region *reg1, *reg2;

  /* Compute distance matrix */
  n = 0;
  /* ... for non-textured images */
  if ((sig1->flags | sig2->flags) & IMAGE_SIG_TEXTURED)
    for (j = 0, reg2 = sig2->reg; j < sig2->len; j++, reg2++)
      for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
        {
	  // FIXME
	  /*uns ds =
	    isqr(reg1->h[0], reg2->h[0]) +
	    isqr(reg1->h[1], reg2->h[1]) +
	    isqr(reg1->h[2], reg2->h[2]);*/
	  uns dt =
	    isqr(reg1->f[0] - reg2->f[0]) +
	    isqr(reg1->f[1] - reg2->f[1]) +
	    isqr(reg1->f[2] - reg2->f[2]) +
	    isqr(reg1->f[3] - reg2->f[3]) +
	    isqr(reg1->f[4] - reg2->f[4]) +
	    isqr(reg1->f[5] - reg2->f[5]);
	  dist[n++] = (CLAMP(dt * 0xffff / (64 * 64 * 6), 0, 0xffff) << 8) + i + (j << 4) ;
        }
  /* ... for textured images (ignore shape properties) */
  else
    for (j = 0, reg2 = sig2->reg; j < sig2->len; j++, reg2++)
      for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
        {
	  uns dt =
	    isqr(reg1->f[0] - reg2->f[0]) +
	    isqr(reg1->f[1] - reg2->f[1]) +
	    isqr(reg1->f[2] - reg2->f[2]) +
	    isqr(reg1->f[3] - reg2->f[3]) +
	    isqr(reg1->f[4] - reg2->f[4]) +
	    isqr(reg1->f[5] - reg2->f[5]);
	  dist[n++] = (CLAMP(dt * 0xffff / (64 * 64 * 6), 0, 0xffff) << 8) + i + (j << 4) ;
        }

  /* One or both signatures have no regions */
  if (!n)
    return 1 << IMAGE_SIG_DIST_SCALE;

  /* Get percentages */
  for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
    p[i] = reg1->wa;
  for (i = 0, reg2 = sig2->reg; i < sig2->len; i++, reg2++)
    q[i] = reg2->wa;

  /* Sort entries in distance matrix */
  image_signatures_dist_2_sort(n, dist);

  /* Compute significance matrix and resulting distance */
  uns sum = 0;
  for (k = 0, l = 128; l; k++)
    {
      i = dist[k] & 15;
      j = (dist[k] >> 4) & 15;
      d = dist[k] >> 8;
      if (p[i] <= q[j])
        {
	  s = p[i];
	  q[j] -= p[i];
	  p[i] = 0;
	}
      else
        {
	  s = q[j];
	  p[i] -= q[j];
	  q[j] = 0;
	}
      l -= s;
      sum += s * d;
      DBG("s[%u][%u]=%u d=%u", i, j, s, d);
    }

  return sum << (IMAGE_SIG_DIST_SCALE - 7 - 16);
}

uns
image_signatures_dist(struct image_signature *sig1, struct image_signature *sig2)
{
  switch (image_sig_compare_method)
    {
      case 1:
	return image_signatures_dist_1(sig1, sig2);
      case 2:
	return image_signatures_dist_2(sig1, sig2);
      default:
	die("Invalid image signatures compare method.");
    }
}

