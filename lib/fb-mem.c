/*
 *	Sherlock Library -- Fast Buffered I/O on Memory Streams
 *
 *	(c) 1997--2000 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <stdio.h>
#include <stdlib.h>

struct memstream {
  unsigned blocksize;
  unsigned uc;
  struct msblock *first;
};

struct msblock {
  struct msblock *next;
  unsigned size;
  byte data[0];
};

static int
fbmem_refill(struct fastbuf *f)
{
  struct memstream *s = f->lldata;
  struct msblock *b = f->llpos;

  if (!b)
    {
      b = s->first;
      if (!b)
	return 0;
    }
  else if (f->buffer == b->data && f->bstop < b->data + b->size)
    {
      f->bstop = b->data + b->size;
      return 1;
    }
  else
    {
      if (!b->next)
	return 0;
      f->pos += b->size;
      b = b->next;
    }
  if (!b->size)
    return 0;
  f->buffer = f->bptr = b->data;
  f->bufend = f->bstop = b->data + b->size;
  f->llpos = b;
  return 1;
}

static void
fbmem_spout(struct fastbuf *f)
{
  struct memstream *s = f->lldata;
  struct msblock *b = f->llpos;
  struct msblock *bb;

  if (b)
    {
      b->size = f->bptr - b->data;
      if (b->size < s->blocksize)
	return;
      f->pos += b->size;
    }
  bb = xmalloc(sizeof(struct msblock) + s->blocksize);
  if (b)
    b->next = bb;
  else
    s->first = bb;
  bb->next = NULL;
  bb->size = 0;
  f->buffer = f->bptr = f->bstop = bb->data;
  f->bufend = bb->data + s->blocksize;
  f->llpos = bb;
}

static void
fbmem_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  struct memstream *m = f->lldata;
  struct msblock *b;
  unsigned int p = 0;

  if (whence != SEEK_SET)
    die("fbmem_seek: only SEEK_SET supported");
  for (b=m->first; b; b=b->next)
    {
      if ((unsigned) pos <= p + b->size) /* <=, because we need to be able to seek just after file end */
	{
	  f->pos = p;
	  f->buffer = b->data;
	  f->bptr = b->data + (pos - p);
	  f->bufend = f->bstop = b->data + b->size;
	  f->llpos = b;
	  return;
	}
      p += b->size;
    }
  die("fbmem_seek to invalid offset");
}

static void
fbmem_close(struct fastbuf *f)
{
  struct memstream *m = f->lldata;
  struct msblock *b;

  if (--m->uc)
    return;

  while (b = m->first)
    {
      m->first = b->next;
      xfree(b);
    }
  xfree(m);
}

struct fastbuf *
fbmem_create(unsigned blocksize)
{
  struct fastbuf *f = xmalloc_zero(sizeof(struct fastbuf));
  struct memstream *m = xmalloc_zero(sizeof(struct memstream));

  m->blocksize = blocksize;
  m->uc = 1;

  f->name = "<fbmem-write>";
  f->lldata = m;
  f->spout = fbmem_spout;
  f->close = fbmem_close;
  return f;
}

struct fastbuf *
fbmem_clone_read(struct fastbuf *b)
{
  struct fastbuf *f = xmalloc_zero(sizeof(struct fastbuf));
  struct memstream *s = b->lldata;

  bflush(b);
  s->uc++;

  f->name = "<fbmem-read>";
  f->lldata = s;
  f->refill = fbmem_refill;
  f->seek = fbmem_seek;
  f->close = fbmem_close;
  return f;
}

#ifdef TEST

int main(void)
{
  struct fastbuf *w, *r;
  int t;

  w = fbmem_create(7);
  r = fbmem_clone_read(w);
  bwrite(w, "12345", 5);
  bwrite(w, "12345", 5);
  printf("<%d>", btell(w));
  bflush(w);
  printf("<%d>", btell(w));
  printf("<%d>", btell(r));
  while ((t = bgetc(r)) >= 0)
    putchar(t);
  printf("<%d>", btell(r));
  bwrite(w, "12345", 5);
  bwrite(w, "12345", 5);
  printf("<%d>", btell(w));
  bclose(w);
  bsetpos(r, 0);
  printf("<!%d>", btell(r));
  while ((t = bgetc(r)) >= 0)
    putchar(t);
  bsetpos(r, 3);
  printf("<!%d>", btell(r));
  while ((t = bgetc(r)) >= 0)
    putchar(t);
  fflush(stdout);
  bclose(r);
  return 0;
}

#endif
