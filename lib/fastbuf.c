/*
 *	Sherlock Library -- Fast Buffered I/O
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <stdlib.h>

void bclose(struct fastbuf *f)
{
  if (f)
    {
      bflush(f);
      if (f->close)
	f->close(f);
    }
}

void bflush(struct fastbuf *f)
{
  if (f->bptr > f->bstop)
    f->spout(f);
  else if (f->bstop > f->buffer)
    f->bptr = f->bstop = f->buffer;
}

inline void bsetpos(struct fastbuf *f, sh_off_t pos)
{
  /* We can optimize seeks only when reading */
  if (pos >= f->pos - (f->bstop - f->buffer) && pos <= f->pos)
    f->bptr = f->bstop + (pos - f->pos);
  else
    {
      bflush(f);
      f->seek(f, pos, SEEK_SET);
    }
}

void bseek(struct fastbuf *f, sh_off_t pos, int whence)
{
  switch (whence)
    {
    case SEEK_SET:
      return bsetpos(f, pos);
    case SEEK_CUR:
      return bsetpos(f, btell(f) + pos);
    case SEEK_END:
      bflush(f);
      f->seek(f, pos, SEEK_END);
      break;
    default:
      die("bseek: invalid whence=%d", whence);
    }
}

int bgetc_slow(struct fastbuf *f)
{
  if (f->bptr < f->bstop)
    return *f->bptr++;
  if (!f->refill(f))
    return EOF;
  return *f->bptr++;
}

int bpeekc_slow(struct fastbuf *f)
{
  if (f->bptr < f->bstop)
    return *f->bptr;
  if (!f->refill(f))
    return EOF;
  return *f->bptr;
}

void bputc_slow(struct fastbuf *f, uns c)
{
  if (f->bptr >= f->bufend)
    f->spout(f);
  *f->bptr++ = c;
}

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

uns bread_slow(struct fastbuf *f, void *b, uns l, uns check)
{
  uns total = 0;
  while (l)
    {
      uns k = f->bstop - f->bptr;

      if (!k)
	{
	  f->refill(f);
	  k = f->bstop - f->bptr;
	  if (!k)
	    break;
	}
      if (k > l)
	k = l;
      memcpy(b, f->bptr, k);
      f->bptr += k;
      b = (byte *)b + k;
      l -= k;
      total += k;
    }
  if (check && total && l)
    die("breadb: short read");
  return total;
}

void bwrite_slow(struct fastbuf *f, void *b, uns l)
{
  while (l)
    {
      uns k = f->bufend - f->bptr;

      if (!k)
	{
	  f->spout(f);
	  k = f->bufend - f->bptr;
	}
      if (k > l)
	k = l;
      memcpy(f->bptr, b, k);
      f->bptr += k;
      b = (byte *)b + k;
      l -= k;
    }
}

byte *					/* Non-standard */
bgets(struct fastbuf *f, byte *b, uns l)
{
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k < 0)
    return NULL;
  while (b < e)
    {
      if (k == '\n' || k < 0)
	{
	  *b = 0;
	  return b;
	}
      *b++ = k;
      k = bgetc(f);
    }
  die("%s: Line too long", f->name);
}

int
bgets_nodie(struct fastbuf *f, byte *b, uns l)
{
  byte *start = b;
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k < 0)
    return 0;
  while (b < e)
    {
      if (k == '\n' || k < 0)
	{
	  *b++ = 0;
	  return b - start;
	}
      *b++ = k;
      k = bgetc(f);
    }
  return -1;
}

byte *
bgets0(struct fastbuf *f, byte *b, uns l)
{
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k < 0)
    return NULL;
  while (b < e)
    {
      if (k <= 0)
	{
	  *b = 0;
	  return b;
	}
      *b++ = k;
      k = bgetc(f);
    }
  die("%s: Line too long", f->name);
}

void
bbcopy_slow(struct fastbuf *f, struct fastbuf *t, uns l)
{
  while (l)
    {
      byte *fptr, *tptr;
      uns favail, tavail, n;

      favail = bdirect_read_prepare(f, &fptr);
      if (!favail)
	{
	  if (l == ~0U)
	    return;
	  die("bbcopy: source exhausted");
	}
      tavail = bdirect_write_prepare(t, &tptr);
      n = MIN(l, favail);
      n = MIN(n, tavail);
      memcpy(tptr, fptr, n);
      bdirect_read_commit(f, fptr + n);
      bdirect_write_commit(t, tptr + n);
      if (l != ~0U)
	l -= n;
    }
}

int
bconfig(struct fastbuf *f, uns item, int value)
{
  return f->config ? f->config(f, item, value) : -1;
}

void
brewind(struct fastbuf *f)
{
  bflush(f);
  bsetpos(f, 0);
}

void
bskip(struct fastbuf *f, uns len)
{
  while (len)
    {
      byte *buf;
      uns l = bdirect_read_prepare(f, &buf);
      l = MIN(l, len);
      bdirect_read_commit(f, buf+l);
      len -= l;
    }
}
