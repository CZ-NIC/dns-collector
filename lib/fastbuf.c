/*
 *	Sherlock Library -- Fast Buffered I/O
 *
 *	(c) 1997--2000 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <stdio.h>
#include <stdlib.h>

void bclose(struct fastbuf *f)
{
  if (f)
    {
      bflush(f);
      f->close(f);
      xfree(f);
    }
}

void bflush(struct fastbuf *f)
{
  if (f->bptr != f->buffer)
    {					/* Have something to flush */
      if (f->bstop > f->buffer)		/* Read data? */
	{
	  f->bptr = f->bstop = f->buffer;
	  f->pos = f->fdpos;
	}
      else				/* Write data... */
	f->spout(f);
    }
}

inline void bsetpos(struct fastbuf *f, sh_off_t pos)
{
  if (pos >= f->pos && (pos <= f->pos + (f->bptr - f->buffer) || pos <= f->pos + (f->bstop - f->buffer)))
    f->bptr = f->buffer + (pos - f->pos);
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

void bputc_slow(struct fastbuf *f, byte c)
{
  if (f->bptr >= f->bufend)
    f->spout(f);
  *f->bptr++ = c;
}

word bgetw_slow(struct fastbuf *f)
{
  word w = bgetc_slow(f);
#ifdef CPU_BIG_ENDIAN
  return (w << 8) | bgetc_slow(f);
#else
  return w | (bgetc_slow(f) << 8);
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

void bputw_slow(struct fastbuf *f, word w)
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
  if (k == EOF)
    return NULL;
  while (b < e)
    {
      if (k == '\n' || k == EOF)
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
bdirect_read(struct fastbuf *f, byte **buf)
{
  int len;

  if (f->bptr == f->bstop && !f->refill(f))
    return EOF;
  *buf = f->bptr;
  len = f->bstop - f->bptr;
  f->bptr += len;
  return len;
}

int
bdirect_write_prepare(struct fastbuf *f, byte **buf)
{
  if (f->bptr == f->bufend)
    f->spout(f);
  *buf = f->bptr;
  return f->bufend - f->bptr;
}

void
bdirect_write_commit(struct fastbuf *f, byte *pos)
{
  f->bptr = pos;
}
