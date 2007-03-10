/*
 *	UCW Library -- Fast Buffered I/O on Static Buffers
 *
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <stdlib.h>

static int
fbbuf_refill(struct fastbuf *f UNUSED)
{
  return 0;
}

static int
fbbuf_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  /* Somebody might want to seek to the end of buffer, try to be nice to him. */
  sh_off_t len = f->bufend - f->buffer;
  if (whence == SEEK_END)
    pos += len;
  ASSERT(pos >= 0 && pos <= len);
  f->bptr = f->buffer + pos;
  f->bstop = f->bufend;
  f->pos = len;
  return 1;
}

void
fbbuf_init_read(struct fastbuf *f, byte *buf, uns size, uns can_overwrite)
{
  f->buffer = f->bptr = buf;
  f->bstop = f->bufend = buf + size;
  f->name = "fbbuf-read";
  f->pos = size;
  f->refill = fbbuf_refill;
  f->spout = NULL;
  f->seek = fbbuf_seek;
  f->close = NULL;
  f->config = NULL;
  f->can_overwrite_buffer = can_overwrite;
}

static void
fbbuf_spout(struct fastbuf *f UNUSED)
{
  die("fbbuf: buffer overflow on write");
}

void
fbbuf_init_write(struct fastbuf *f, byte *buf, uns size)
{
  f->buffer = f->bstop = f->bptr = buf;
  f->bufend = buf + size;
  f->name = "fbbuf-write";
  f->pos = size;
  f->refill = NULL;
  f->spout = fbbuf_spout;
  f->seek = NULL;
  f->close = NULL;
  f->config = NULL;
  f->can_overwrite_buffer = 0;
}
