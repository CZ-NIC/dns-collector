#ifndef SIG_EXPLAIN

#define MSG(x...) do{}while(0)

static uns
image_signatures_dist_2(struct image_signature *sig1, struct image_signature *sig2)

#else

#define MSG(x...) bprintf(fb, x)

static void
dump_signature(struct image_signature *sig, struct fastbuf *fb)
{
  MSG("signature: flags=0x%x df=%u dh=%u f=(%u", sig->flags, sig->df, sig->dh, sig->vec.f[0]);
  for (uns i = 1; i < IMAGE_VEC_F; i++)
    MSG(" %u", sig->vec.f[i]);
  MSG(")\n");
  for (uns j = 0; j < sig->len; j++)
    {
      struct image_region *reg = sig->reg + j;
      MSG("region %u: wa=%u wb=%u f=(%u", j, reg->wa, reg->wb, reg->f[0]);
      for (uns i = 1; i < IMAGE_VEC_F; i++)
	MSG(" %u", reg->f[i]);
      MSG(") h=(%u", reg->h[0]);
      for (uns i = 1; i < IMAGE_REG_H; i++)
	MSG(" %u", reg->h[i]);
      MSG(")\n");
    }
}

static uns
image_signatures_dist_2_explain(struct image_signature *sig1, struct image_signature *sig2, struct fastbuf *fb)

#endif
{
  DBG("image_signatures_dist_2()");

  uns dist[IMAGE_REG_MAX * IMAGE_REG_MAX], p[IMAGE_REG_MAX], q[IMAGE_REG_MAX];
  uns n, i, j, k, l, s, d;
  struct image_region *reg1, *reg2;

#ifdef SIG_EXPLAIN
  dump_signature(sig1, fb);
  dump_signature(sig2, fb);
#endif

  /* Compute distance matrix */
  n = 0;
  MSG("Distance matrix:\n");
  /* ... for non-textured images */
  if (!((sig1->flags | sig2->flags) & IMAGE_SIG_TEXTURED))
    for (j = 0, reg2 = sig2->reg; j < sig2->len; j++, reg2++)
      for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
        {
	  uns ds =
	    isqr((int)reg1->h[0] - (int)reg2->h[0]) +
	    isqr((int)reg1->h[1] - (int)reg2->h[1]) +
	    isqr((int)reg1->h[2] - (int)reg2->h[2]);
	  uns dt =
	    isqr((int)reg1->f[0] - (int)reg2->f[0]) +
	    isqr((int)reg1->f[1] - (int)reg2->f[1]) +
	    isqr((int)reg1->f[2] - (int)reg2->f[2]) +
	    isqr((int)reg1->f[3] - (int)reg2->f[3]) +
	    isqr((int)reg1->f[4] - (int)reg2->f[4]) +
	    isqr((int)reg1->f[5] - (int)reg2->f[5]);
	  if (ds < 1000)
	    dt *= 8;
	  else if (ds < 10000)
	    dt *= 12;
	  else
	    dt *= 16;
	  DBG("[%u][%u] ... dt=%u ds=%u", i, j, dt, ds);
	  dist[n++] = (dt << 8) + i + (j << 4);
	  MSG("[%u, %u] ... dt=%u ds=%u\n", i, j, dt, ds);
        }
  /* ... for textured images (ignore shape properties) */
  else
    for (j = 0, reg2 = sig2->reg; j < sig2->len; j++, reg2++)
      for (i = 0, reg1 = sig1->reg; i < sig1->len; i++, reg1++)
        {
	  uns dt =
	    isqr((int)reg1->f[0] - (int)reg2->f[0]) +
	    isqr((int)reg1->f[1] - (int)reg2->f[1]) +
	    isqr((int)reg1->f[2] - (int)reg2->f[2]) +
	    isqr((int)reg1->f[3] - (int)reg2->f[3]) +
	    isqr((int)reg1->f[4] - (int)reg2->f[4]) +
	    isqr((int)reg1->f[5] - (int)reg2->f[5]);
	  dist[n++] = (dt << 12) + i + (j << 4);
	  MSG("[%u, %u] ... dt=%u\n", i, j, dt);
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
  image_signatures_dist_2_sort(n, dist);

  /* Compute significance matrix and resulting distance */
  uns sum = 0;
  MSG("Significance matrix:\n");
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
      MSG("[%u, %u]=%u d=%u\n", i, j, s, d);
    }

  return sum;
}

#undef SIG_EXPLAIN
#undef MSG
