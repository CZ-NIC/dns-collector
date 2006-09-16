#ifdef EXPLAIN
#  define MSG(x...) do{ line += sprintf(line, x); }while(0)
#  define LINE do{ line = buf; msg(line, param); }while(0)

static void
explain_signature(struct image_signature *sig, void (*msg)(byte *text, void *param), void *param)
{
  byte buf[1024], *line = buf;
  MSG("signature: flags=0x%x df=%u dh=%u f=(%u", sig->flags, sig->df, sig->dh, sig->vec.f[0]);
  for (uns i = 1; i < IMAGE_VEC_F; i++)
    MSG(" %u", sig->vec.f[i]);
  MSG(")");
  LINE;
  for (uns j = 0; j < sig->len; j++)
    {
      struct image_region *reg = sig->reg + j;
      MSG("region %u: wa=%u wb=%u f=(%u", j, reg->wa, reg->wb, reg->f[0]);
      for (uns i = 1; i < IMAGE_VEC_F; i++)
	MSG(" %u", reg->f[i]);
      MSG(") h=(%u", reg->h[0]);
      for (uns i = 1; i < IMAGE_REG_H; i++)
	MSG(" %u", reg->h[i]);
      MSG(")");
      LINE;
    }
}

#else
#  define MSG(x...) do{}while(0)
#  define LINE do{}while(0)
#endif

#ifndef EXPLAIN
static uns
image_signatures_dist_integrated(struct image_signature *sig1, struct image_signature *sig2)
#else
static uns
image_signatures_dist_integrated_explain(struct image_signature *sig1, struct image_signature *sig2, void (*msg)(byte *text, void *param), void *param)
#endif
{
  DBG("image_signatures_dist_integrated()");

  uns dist[IMAGE_REG_MAX * IMAGE_REG_MAX], p[IMAGE_REG_MAX], q[IMAGE_REG_MAX];
  uns n, i, j, k, l, s, d;
  struct image_region *reg1, *reg2;
#ifdef EXPLAIN
  byte buf[1024], *line = buf;
  explain_signature(sig1, msg, param);
  explain_signature(sig2, msg, param);
#endif

  /* FIXME: do not mux textured and non-textured images (should be split in clusters tree) */
  if ((sig1->flags ^ sig2->flags) & IMAGE_SIG_TEXTURED)
    {
      MSG("Textured vs non-textured");
      LINE;
      return ~0U;
    }

  /* Compute distance matrix */
  n = 0;
  MSG("Distance matrix:");
  LINE;
  /* ... for non-textured images */
  if (!((sig1->flags | sig2->flags) & IMAGE_SIG_TEXTURED))
    for (j = 0, reg2 = sig2->reg; j < sig2->len; j++, reg2++)
      for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
        {
	  uns ds =
	    image_sig_cmp_features_weights[6] * isqr((int)reg1->h[0] - (int)reg2->h[0]) +
	    image_sig_cmp_features_weights[7] * isqr((int)reg1->h[1] - (int)reg2->h[1]) +
	    image_sig_cmp_features_weights[8] * isqr((int)reg1->h[2] - (int)reg2->h[2]);
	  uns dt =
	    image_sig_cmp_features_weights[0] * isqr((int)reg1->f[0] - (int)reg2->f[0]) +
	    image_sig_cmp_features_weights[1] * isqr((int)reg1->f[1] - (int)reg2->f[1]) +
	    image_sig_cmp_features_weights[2] * isqr((int)reg1->f[2] - (int)reg2->f[2]) +
	    image_sig_cmp_features_weights[3] * isqr((int)reg1->f[3] - (int)reg2->f[3]) +
	    image_sig_cmp_features_weights[4] * isqr((int)reg1->f[4] - (int)reg2->f[4]) +
	    image_sig_cmp_features_weights[5] * isqr((int)reg1->f[5] - (int)reg2->f[5]);
	  if (ds < 1000)
	    dt *= 8;
	  else if (ds < 10000)
	    dt *= 12;
	  else
	    dt *= 16;
	  dist[n++] = (dt << 8) + i + (j << 4);
	  DBG("[%u, %u] dt=%u ds=%u", i, j, dt, ds);
	  MSG("[%u, %u] dt=%u ds=%u", i, j, dt, ds);
	  LINE;
        }
  /* ... for textured images (ignore shape properties) */
  else
    for (j = 0, reg2 = sig2->reg; j < sig2->len; j++, reg2++)
      for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
        {
	  uns dt =
	    image_sig_cmp_features_weights[0] * isqr((int)reg1->f[0] - (int)reg2->f[0]) +
	    image_sig_cmp_features_weights[1] * isqr((int)reg1->f[1] - (int)reg2->f[1]) +
	    image_sig_cmp_features_weights[2] * isqr((int)reg1->f[2] - (int)reg2->f[2]) +
	    image_sig_cmp_features_weights[3] * isqr((int)reg1->f[3] - (int)reg2->f[3]) +
	    image_sig_cmp_features_weights[4] * isqr((int)reg1->f[4] - (int)reg2->f[4]) +
	    image_sig_cmp_features_weights[5] * isqr((int)reg1->f[5] - (int)reg2->f[5]);
	  dist[n++] = (dt << 12) + i + (j << 4);
	  DBG("[%u, %u] dt=%u", i, j, dt);
	  MSG("[%u, %u] dt=%u", i, j, dt);
	  LINE;
        }

  /* One or both signatures have no regions */
  if (!n)
    return 0xffffffff;

  /* Get percentages */
  for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
    p[i] = reg1->wb;
  for (i = 0, reg2 = sig2->reg; i < sig2->len; i++, reg2++)
    q[i] = reg2->wb;

  /* Sort entries in distance matrix */
  image_signatures_dist_integrated_sort(n, dist);

  /* Compute significance matrix and resulting distance */
  uns sum = 0;
  MSG("Significance matrix:");
  LINE;
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
      DBG("[%u, %u] s=%u d=%u", i, j, s, d);
      MSG("[%u, %u] s=%u d=%u", i, j, s, d);
      LINE;
    }

  return sum;
}

#ifndef EXPLAIN
static uns
image_signatures_dist_fuzzy(struct image_signature *sig1, struct image_signature *sig2)
#else
static uns
image_signatures_dist_fuzzy_explain(struct image_signature *sig1, struct image_signature *sig2, void (*msg)(byte *text, void *param), void *param)
#endif
{
  DBG("image_signatures_dist_fuzzy()");

#ifdef EXPLAIN
  byte buf[1024], *line = buf;
  explain_signature(sig1, msg, param);
  explain_signature(sig2, msg, param);
#endif

  /* FIXME: do not mux textured and non-textured images (should be split in clusters tree) */
  if ((sig1->flags ^ sig2->flags) & IMAGE_SIG_TEXTURED)
    {
      MSG("Textured vs non-textured");
      LINE;
      return ~0U;
    }

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
  byte buf2[2 * IMAGE_REG_MAX * 16 + 3], *b = buf2;
  for (uns i = 0; i < cnt1 + cnt2; i++)
    {
      if (i)
	*b++ = ' ';
      if (i == cnt1)
	*b++ = '~', *b++ = ' ';
      b += sprintf(b, "%.4f", (double)lf[i] / 0x10000);
    }
  *b = 0;
  DBG("Lf=(%s)", buf2);
  b = buf2;
  for (uns i = 0; i < cnt1 + cnt2; i++)
    {
      if (i)
	*b++ = ' ';
      if (i == cnt1)
	*b++ = '~', *b++ = ' ';
      b += sprintf(b, "%.4f", (double)lh[i] / 0x10000);
    }
  *b = 0;
  DBG("Lh=(%s)", buf2);
  DBG("Lfm=%.4f", lfs / (double)(1 << (3 + 8 + 16)));
  DBG("Lhm=%.4f", lhs / (double)(1 << (8 + 16)));
  DBG("measure=%.4f", measure / (double)(1 << (3 + 3 + 8 + 16)));
#endif

  return (1 << (3 + 3 + 8 + 16)) - measure;
}

#undef EXPLAIN
#undef MSG
#undef LINE
