/*
 *	UCW Library -- Fast Buffered I/O: Binary Numbers
 *
 *	(c) 1997--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/ff-binary.h"

int bgetw_slow(struct fastbuf *f)
{
  int w1, w2;
  w1 = bgetc_slow(f);
  if (w1 < 0)
    return w1;
  w2 = bgetc_slow(f);
  if (w2 < 0)
    return w2;
#ifdef CPU_BIG_ENDIAN
  return (w1 << 8) | w2;
#else
  return w1 | (w2 << 8);
#endif
}

u32 bgetl_slow(struct fastbuf *f)
{
  u32 l = bgetc_slow(f);
#ifdef CPU_BIG_ENDIAN
  l = (l << 8) | bgetc_slow(f);
  l = (l << 8) | bgetc_slow(f);
  return (l << 8) | bgetc_slow(f);
#else
  l = (bgetc_slow(f) << 8) | l;
  l = (bgetc_slow(f) << 16) | l;
  return (bgetc_slow(f) << 24) | l;
#endif
}

u64 bgetq_slow(struct fastbuf *f)
{
  u32 l, h;
#ifdef CPU_BIG_ENDIAN
  h = bgetl_slow(f);
  l = bgetl_slow(f);
#else
  l = bgetl_slow(f);
  h = bgetl_slow(f);
#endif
  return ((u64) h << 32) | l;
}

u64 bget5_slow(struct fastbuf *f)
{
  u32 l, h;
#ifdef CPU_BIG_ENDIAN
  h = bgetc_slow(f);
  l = bgetl_slow(f);
#else
  l = bgetl_slow(f);
  h = bgetc_slow(f);
#endif
  return ((u64) h << 32) | l;
}

void bputw_slow(struct fastbuf *f, uns w)
{
#ifdef CPU_BIG_ENDIAN
  bputc_slow(f, w >> 8);
  bputc_slow(f, w);
#else
  bputc_slow(f, w);
  bputc_slow(f, w >> 8);
#endif
}

void bputl_slow(struct fastbuf *f, u32 l)
{
#ifdef CPU_BIG_ENDIAN
  bputc_slow(f, l >> 24);
  bputc_slow(f, l >> 16);
  bputc_slow(f, l >> 8);
  bputc_slow(f, l);
#else
  bputc_slow(f, l);
  bputc_slow(f, l >> 8);
  bputc_slow(f, l >> 16);
  bputc_slow(f, l >> 24);
#endif
}

void bputq_slow(struct fastbuf *f, u64 q)
{
#ifdef CPU_BIG_ENDIAN
  bputl_slow(f, q >> 32);
  bputl_slow(f, q);
#else
  bputl_slow(f, q);
  bputl_slow(f, q >> 32);
#endif
}

void bput5_slow(struct fastbuf *f, u64 o)
{
  u32 hi = o >> 32;
  u32 low = o;
#ifdef CPU_BIG_ENDIAN
  bputc_slow(f, hi);
  bputl_slow(f, low);
#else
  bputl_slow(f, low);
  bputc_slow(f, hi);
#endif
}
