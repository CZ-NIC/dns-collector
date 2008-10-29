/*
 *	UCW Library -- Fast Buffered I/O
 *
 *	(c) 1997--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/fastbuf.h"
#include "ucw/respool.h"
#include "ucw/trans.h"

#include <stdio.h>
#include <stdlib.h>

void bclose(struct fastbuf *f)
{
  if (f)
    {
      if (!(f->flags & FB_DEAD))
        bflush(f);
      if (f->close)
	f->close(f);
      if (f->res)
	res_drop(f->res);
    }
}

void NONRET bthrow(struct fastbuf *f, const char *id, const char *fmt, ...)
{
  ASSERT(!(f->flags & FB_DEAD));
  f->flags |= FB_DEAD;
  va_list args;
  va_start(args, fmt);
  trans_vthrow(id, f, fmt, args);
}

int brefill(struct fastbuf *f, int allow_eof)
{
  ASSERT(f->bptr >= f->bstop);
  if (!f->refill)
    bthrow(f, "fb.read", "Stream not readable");
  if (f->refill(f))
    {
      ASSERT(f->bptr < f->bstop);
      return 1;
    }
  else
    {
      if (!allow_eof && (f->flags & FB_DIE_ON_EOF))
	bthrow(f, "fb.eof", "Unexpected EOF");
      ASSERT(f->bptr == f->bstop);
      return 0;
    }
}

void bspout(struct fastbuf *f)
{
  ASSERT(f->bptr > f->bstop || f->bptr >= f->bufend);
  if (!f->spout)
    bthrow(f, "fb.write", "Stream not writeable");
  f->spout(f);
  ASSERT(f->bptr < f->bufend);
}

void bflush(struct fastbuf *f)
{
  if (f->bptr > f->bstop)
    bspout(f);
  else if (f->bstop > f->buffer)
    f->bptr = f->bstop = f->buffer;
}

inline void bsetpos(struct fastbuf *f, ucw_off_t pos)
{
  /* We can optimize seeks only when reading */
  if (pos >= f->pos - (f->bstop - f->buffer) && pos <= f->pos)
    f->bptr = f->bstop + (pos - f->pos);
  else
    {
      bflush(f);
      if (!f->seek || !f->seek(f, pos, SEEK_SET))
	bthrow(f, "fb.seek", "Stream not seekable");
    }
}

void bseek(struct fastbuf *f, ucw_off_t pos, int whence)
{
  switch (whence)
    {
    case SEEK_SET:
      return bsetpos(f, pos);
    case SEEK_CUR:
      return bsetpos(f, btell(f) + pos);
    case SEEK_END:
      bflush(f);
      if (!f->seek || !f->seek(f, pos, SEEK_END))
	bthrow(f, "fb.seek", "Stream not seekable");
      break;
    default:
      die("bseek: invalid whence=%d", whence);
    }
}

int bgetc_slow(struct fastbuf *f)
{
  if (f->bptr < f->bstop)
    return *f->bptr++;
  if (!brefill(f, 0))
    return -1;
  return *f->bptr++;
}

int bpeekc_slow(struct fastbuf *f)
{
  if (f->bptr < f->bstop)
    return *f->bptr;
  if (!brefill(f, 0))
    return -1;
  return *f->bptr;
}

int beof_slow(struct fastbuf *f)
{
  return f->bptr >= f->bstop && !brefill(f, 1);
}

void bputc_slow(struct fastbuf *f, uns c)
{
  if (f->bptr >= f->bufend)
    bspout(f);
  *f->bptr++ = c;
}

uns bread_slow(struct fastbuf *f, void *b, uns l, uns check)
{
  uns total = 0;
  while (l)
    {
      uns k = f->bstop - f->bptr;

      if (!k)
	{
	  brefill(f, check);
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
    bthrow(f, "fb.read", "breadb: short read");
  return total;
}

void bwrite_slow(struct fastbuf *f, const void *b, uns l)
{
  while (l)
    {
      uns k = f->bufend - f->bptr;

      if (!k)
	{
	  bspout(f);
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

ucw_off_t
bfilesize(struct fastbuf *f)
{
  if (!f)
    return 0;
  ucw_off_t pos = btell(f);
  bflush(f);
  if (!f->seek(f, 0, SEEK_END))
    return -1;
  ucw_off_t len = btell(f);
  bsetpos(f, pos);
  return len;
}

/* Resources */

static void
fb_res_detach(struct resource *r)
{
  struct fastbuf *f = r->priv;
  f->res = NULL;
}

static void
fb_res_free(struct resource *r)
{
  struct fastbuf *f = r->priv;
  f->res = NULL;
  bclose(f);
}

static void
fb_res_dump(struct resource *r)
{
  struct fastbuf *f = r->priv;
  printf(" name=%s", f->name);
}

static const struct res_class fb_res_class = {
  .name = "fastbuf",
  .detach = fb_res_detach,
  .dump = fb_res_dump,
  .free = fb_res_free,
};

void
fb_tie(struct fastbuf *f)
{
  f->res = res_new(&fb_res_class, f);
}
