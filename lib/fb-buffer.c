/*
 *	Sherlock Library -- Fast Buffered I/O on Static Buffers
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

static int
fbbuf_refill(struct fastbuf *f UNUSED)
{
  return 0;
}

void
fbbuf_init_read(struct fastbuf *f, byte *buf, uns size)
{
  f->buffer = f->bptr = buf;
  f->bstop = f->bufend = buf + size;
  f->name = "fbbuf-read";
  f->pos = size;
  f->refill = fbbuf_refill;
  f->spout = NULL;
  f->seek = NULL;
  f->close = NULL;
  f->config = NULL;
}

static void
fbbuf_spout(struct fastbuf *f UNUSED)
{
  die("fbbuf: buffer overflow on write");
}

static int
fbbuf_config(struct fastbuf *f UNUSED, uns item, int value UNUSED)
{
  switch (item)
    {
    case BCONFIG_CAN_OVERWRITE:
      return 1;
    default:
      return -1;
    }
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
  f->config = fbbuf_config;
}
