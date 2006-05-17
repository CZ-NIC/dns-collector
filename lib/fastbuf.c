/*
 *	UCW Library -- Fast Buffered I/O
 *
 *	(c) 1997--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/mempool.h"

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

uns
bgets_bb(struct fastbuf *f, bb_t *bb)
{
  byte *buf = bb->ptr, *src;
  uns len = 0, buf_len = bb->len, src_len = bdirect_read_prepare(f, &src);
  while (src_len)
    {
      uns cnt = MIN(src_len, buf_len);
      for (uns i = cnt; i--;)
        {
	  if (*src == '\n')
	    {
	      *buf = 0;
	      return buf - bb->ptr;
	    }
	  *buf++ = *src++;
	}
      len += cnt;
      if (cnt == src_len)
	src_len = bdirect_read_prepare(f, &src);
      else
	src_len -= cnt;
      if (cnt == buf_len)
        {
	  bb_do_grow(bb, len + 1);
	  buf = bb->ptr + len;
	  buf_len = bb->len - len;
	}
      else
	buf_len -= cnt;
    }
  *buf = 0;
  return len;
}

byte *
bgets_mp(struct mempool *mp, struct fastbuf *f)
{
#define BLOCK_SIZE 4096
  struct block {
    struct block *prev;
    byte data[BLOCK_SIZE];
  } *blocks = NULL;
  uns sum = 0;
  for (;;)
    {
      struct block *new_block = alloca(sizeof(struct block));
      byte *b = new_block->data, *e = b + BLOCK_SIZE;
      while (b < e)
        {
	  int k = bgetc(f);
	  if (k == '\n' || k < 0)
	    {
	      uns len = b - new_block->data;
	      byte *result = mp_alloc(mp, sum + len + 1) + sum;
	      result[len] = 0;
	      memcpy(result, new_block->data, len);
	      while (blocks)
	        {
		  result -= BLOCK_SIZE;
		  memcpy(result, blocks->data, BLOCK_SIZE);
		  blocks = blocks->prev;
		}
	      return result;
	    }
	  *b++ = k;
	}
      new_block->prev = blocks;
      blocks = new_block;
      sum += BLOCK_SIZE;
    }
#undef BLOCK_SIZE
}

int
bgets_stk_step(struct fastbuf *f, byte *old_buf, byte *buf, uns len)
{
  if (old_buf)
    {
      len = len >> 1;
      memcpy(buf, old_buf, len);
      buf += len;
    }
  while (len--)
    {
      int k = bgetc(f);
      if (k == '\n' || k < 0)
	return *buf = 0;
      *buf++ = k;
    }
  return 1;
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

int
bskip_slow(struct fastbuf *f, uns len)
{
  while (len)
    {
      byte *buf;
      uns l = bdirect_read_prepare(f, &buf);
      if (!l)
	return 0;
      l = MIN(l, len);
      bdirect_read_commit(f, buf+l);
      len -= l;
    }
  return 1;
}

sh_off_t
bfilesize(struct fastbuf *f)
{
  if (!f)
    return 0;
  sh_off_t pos = btell(f);
  bseek(f, 0, SEEK_END);
  sh_off_t len = btell(f);
  bsetpos(f, pos);
  return len;
}
