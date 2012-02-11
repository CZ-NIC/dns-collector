/*
 *	UCW Library -- Fast Buffered I/O on Growing Buffers
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/fastbuf.h"
#include "ucw/mempool.h"

#include <stdio.h>
#include <stdlib.h>

struct fb_gbuf {
  struct fastbuf fb;
  struct mempool *mp;
  byte *end;
};
#define FB_GBUF(f) ((struct fb_gbuf *)(f))

static int fbgrow_refill(struct fastbuf *b)
{
  b->bstop = FB_GBUF(b)->end;
  b->pos = b->bstop - b->buffer;
  return b->bstop > b->bptr;
}

static void fbgrow_spout(struct fastbuf *b)
{
  if (b->bptr == b->bufend)
    {
      uns len = b->bufend - b->buffer;
      if (FB_GBUF(b)->mp)
	{
	  byte *old = b->buffer;
	  b->buffer = mp_alloc(FB_GBUF(b)->mp, 2 * len);
	  memcpy(b->buffer, old, len);
	}
      else
        b->buffer = xrealloc(b->buffer, 2 * len);
      b->bufend = b->buffer + 2 * len;
      FB_GBUF(b)->end = b->bptr = b->buffer + len;
    }
  else if (FB_GBUF(b)->end < b->bptr)
    FB_GBUF(b)->end = b->bptr;
  b->bstop = b->buffer;
  b->pos = 0;
}

static int fbgrow_seek(struct fastbuf *b, ucw_off_t pos, int whence)
{
  ucw_off_t len = FB_GBUF(b)->end - b->buffer;
  if (whence == SEEK_END)
    pos += len;
  if (pos < 0 || pos > len)
    bthrow(b, "seek", "Seek out of range");
  b->bptr = b->buffer + pos;
  b->bstop = b->buffer;
  b->pos = 0;
  return 1;
}

static void fbgrow_close(struct fastbuf *b)
{
  xfree(b->buffer);
  xfree(b);
}

struct fastbuf *fbgrow_create_mp(struct mempool *mp, unsigned basic_size)
{
  ASSERT(basic_size);
  struct fastbuf *b;
  if (mp)
    {
      b = mp_alloc_zero(mp, sizeof(struct fb_gbuf));
      b->buffer = mp_alloc(mp, basic_size);
      FB_GBUF(b)->mp = mp;
    }
  else
    {
      b = xmalloc_zero(sizeof(struct fb_gbuf));
      b->buffer = xmalloc(basic_size);
      b->close = fbgrow_close;
    }
  b->bufend = b->buffer + basic_size;
  b->bptr = b->bstop = b->buffer;
  b->name = "<fbgbuf>";
  b->refill = fbgrow_refill;
  b->spout = fbgrow_spout;
  b->seek = fbgrow_seek;
  b->can_overwrite_buffer = 1;
  return b;
}

struct fastbuf *fbgrow_create(unsigned basic_size)
{
  return fbgrow_create_mp(NULL, basic_size);
}

void fbgrow_reset(struct fastbuf *b)
{
  FB_GBUF(b)->end = b->bptr = b->bstop = b->buffer;
  b->pos = 0;
}

void fbgrow_rewind(struct fastbuf *b)
{
  brewind(b);
}

uns fbgrow_get_buf(struct fastbuf *b, byte **buf)
{
  byte *end = FB_GBUF(b)->end;
  end = MAX(end, b->bptr);
  if (buf)
    *buf = b->buffer;
  return end - b->buffer;
}

#ifdef TEST

int main(void)
{
  struct fastbuf *f;
  uns t;

  f = fbgrow_create(3);
  for (uns i=0; i<5; i++)
    {
      fbgrow_reset(f);
      bwrite(f, "12345", 5);
      bwrite(f, "12345", 5);
      printf("<%d>", (int)btell(f));
      bflush(f);
      printf("<%d>", (int)btell(f));
      fbgrow_rewind(f);
      printf("<%d>", (int)btell(f));
      while ((t = bgetc(f)) != ~0U)
	putchar(t);
      printf("<%d>", (int)btell(f));
      fbgrow_rewind(f);
      bseek(f, -1, SEEK_END);
      printf("<%d>", (int)btell(f));
      while ((t = bgetc(f)) != ~0U)
	putchar(t);
      printf("<%d>\n", (int)btell(f));
    }
  bclose(f);
  return 0;
}

#endif
