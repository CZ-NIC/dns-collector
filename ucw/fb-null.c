/*
 *	UCW Library -- Null fastbuf
 *
 *	(c) 2014 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>

#include <stdio.h>

static void fbnull_close(struct fastbuf *b)
{
  xfree(b);
}

struct fastbuf *fbnull_open(uns bufsize)
{
  struct fastbuf *b = xmalloc(sizeof(*b) + bufsize);
  bzero(b, sizeof(*b));
  b->close = fbnull_close;
  fbnull_start(b, (byte *)(b + 1), bufsize);
  return b;
}

static int fbnull_refill(struct fastbuf *b UNUSED)
{
  return 0;
}

static void fbnull_spout(struct fastbuf *b)
{
  b->pos += b->bptr - b->bstop;
  b->bptr = b->bstop;
}

static int fbnull_seek(struct fastbuf *b, ucw_off_t pos, int whence)
{
  b->pos = (whence == SEEK_END) ? 0 : pos;
  b->bptr = b->bstop;
  return 1;
}

void fbnull_start(struct fastbuf *b, byte *buf, uns bufsize)
{
  ASSERT(buf && bufsize);
  b->pos = btell(b);
  b->buffer = b->bptr = b->bstop = buf;
  b->bufend = buf + bufsize;
  b->refill = fbnull_refill;
  b->spout = fbnull_spout;
  b->seek = fbnull_seek;
  b->can_overwrite_buffer = 2;
}

bool fbnull_test(struct fastbuf *b)
{
  return b->refill == fbnull_refill;
}

#ifdef TEST
int main(void)
{
  struct fastbuf *b = fbnull_open(7);
  for (uns i = 0; i < 100; i++)
    {
      if (btell(b) != i * 10)
	ASSERT(0);
      if (bgetc(b) >= 0)
        ASSERT(0);
      bputs(b, "0123456789");
      bflush(b);
    }
  if (bfilesize(b) != 0)
    ASSERT(0);
  if (btell(b) != 100 * 10)
    ASSERT(0);
  bclose(b);
  return 0;
}
#endif
