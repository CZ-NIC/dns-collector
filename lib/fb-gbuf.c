/*
 *	UCW Library -- Fast Buffered I/O on Growing Buffers
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <stdlib.h>

struct fb_gbuf {
  struct fastbuf fb;
  byte *last_written;
};
#define FB_GBUF(f) ((struct fb_gbuf *)(f)->is_fastbuf)

static int
fbgbuf_refill(struct fastbuf *b)
{
  if (b->bstop != FB_GBUF(b)->last_written)
    {
      /* There was an intervening flush */
      b->bstop = FB_GBUF(b)->last_written;
      b->pos = b->bstop - b->buffer;
      return 1;
    }
  /* We are at the end */
  return 0;
}

static void
fbgbuf_spout(struct fastbuf *b)
{
  if (b->bptr >= b->bufend)
    {
      uns len = b->bufend - b->buffer;
      b->buffer = xrealloc(b->buffer, 2*len);
      b->bufend = b->buffer + 2*len;
      b->bstop = b->buffer;
      b->bptr = b->buffer + len;
    }
}

static void
fbgbuf_seek(struct fastbuf *b, sh_off_t pos, int whence)
{
  ASSERT(FB_GBUF(b)->last_written);	/* Seeks allowed only in read mode */
  sh_off_t len = FB_GBUF(b)->last_written - b->buffer;
  if (whence == SEEK_END)
    pos += len;
  ASSERT(pos >= 0 && pos <= len);
  b->bptr = b->buffer + pos;
  b->bstop = FB_GBUF(b)->last_written;
  b->pos = len;
}

static void
fbgbuf_close(struct fastbuf *b)
{
  xfree(b->buffer);
  xfree(b);
}

struct fastbuf *
fbgbuf_create(unsigned basic_size)
{
  struct fastbuf *b = xmalloc_zero(sizeof(struct fb_gbuf));
  b->buffer = xmalloc(basic_size);
  b->bufend = b->buffer + basic_size;
  b->bptr = b->bstop = b->buffer;
  b->name = "<fbgbuf>";
  b->refill = fbgbuf_refill;
  b->spout = fbgbuf_spout;
  b->seek = fbgbuf_seek;
  b->close = fbgbuf_close;
  b->can_overwrite_buffer = 1;
  return b;
}

void
fbgbuf_write(struct fastbuf *b)
{
  b->bptr = b->bstop = b->buffer;
  b->pos = 0;
  FB_GBUF(b)->last_written = NULL;
}

void
fbgbuf_rewind(struct fastbuf *b)
{
  if (!FB_GBUF(b)->last_written)
    {
      /* Last operation was a write, so remember the end position */
      FB_GBUF(b)->last_written = b->bptr;
    }
  b->bptr = b->buffer;
  b->bstop = FB_GBUF(b)->last_written;
  b->pos = b->bstop - b->buffer;
}

#ifdef TEST

int main(void)
{
  struct fastbuf *f;
  int t;

  f = fbgbuf_create(3);
  for (uns i=0; i<5; i++)
    {
      fbgbuf_write(f);
      bwrite(f, "12345", 5);
      bwrite(f, "12345", 5);
      printf("<%d>", (int)btell(f));
      bflush(f);
      printf("<%d>", (int)btell(f));
      fbgbuf_rewind(f);
      printf("<%d>", (int)btell(f));
      while ((t = bgetc(f)) >= 0)
	putchar(t);
      printf("<%d>", (int)btell(f));
      fbgbuf_rewind(f);
      bseek(f, -1, SEEK_END);
      printf("<%d>", (int)btell(f));
      while ((t = bgetc(f)) >= 0)
	putchar(t);
      printf("<%d>\n", (int)btell(f));
    }
  bclose(f);
  return 0;
}

#endif
