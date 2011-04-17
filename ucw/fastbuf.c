/*
 *	UCW Library -- Fast Buffered I/O
 *
 *	(c) 1997--2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "ucw/fastbuf.h"
#include "ucw/respool.h"
#include "ucw/trans.h"
#include "ucw/stkstring.h"

#include <stdio.h>
#include <stdlib.h>

void bclose(struct fastbuf *f)
{
  if (f)
    {
      bflush(f);
      if (f->res)
	{
	  res_drop(f->res);
	  f->res = NULL;
	}
      DBG("FB: closing", f);
      if (f->close)
	f->close(f); /* Should always free all internal resources, even if it throws an exception */
    }
}

void NONRET bthrow(struct fastbuf *f, const char *id, const char *fmt, ...)
{
  ASSERT(!(f->flags & FB_DEAD)); /* Only one bthrow() is allowed before bclose() */
  DBG("FB: throwing %s", id);
  va_list args;
  va_start(args, fmt);
  if (!f->res)
    die("Fastbuf %s error: %s", f->name ? : "<fb>", stk_vprintf(fmt, args));
  f->flags |= FB_DEAD;
  f->bptr = f->bstop = f->bufend; /* Reset the buffer to guard consecutive seek/read/write */
  trans_vthrow(id, f, fmt, args);
}

int brefill(struct fastbuf *f, int allow_eof)
{
  DBG("FB: refill");
  ASSERT(!(f->flags & FB_DEAD) && f->buffer <= f->bstop && f->bstop <= f->bptr && f->bptr <= f->bufend);
  if (!f->refill)
    bthrow(f, "fb.read", "Stream not readable");
  if (f->refill(f))
    {
      ASSERT(f->buffer <= f->bptr && f->bptr < f->bstop && f->bstop <= f->bufend);
      return 1;
    }
  else
    {
      ASSERT(f->buffer <= f->bptr && f->bptr == f->bstop && f->bstop <= f->bufend);
      if (!allow_eof && (f->flags & FB_DIE_ON_EOF))
	bthrow(f, "fb.eof", "Unexpected EOF");
      return 0;
    }
}

static void do_spout(struct fastbuf *f)
{
  DBG("FB: spout");
  ASSERT(!(f->flags & FB_DEAD) && f->buffer <= f->bstop && f->bstop <= f->bptr && f->bptr <= f->bufend); /* Check write mode possibly with unflushed data */
  if (!f->spout)
    bthrow(f, "fb.write", "Stream not writeable");
  f->spout(f);
  ASSERT(f->buffer <= f->bstop && f->bstop <= f->bptr && f->bptr <= f->bufend);
}

void bspout(struct fastbuf *f)
{
  do_spout(f);
  if (f->bstop == f->bufend)
    {
      do_spout(f);
      ASSERT(f->bstop < f->bufend);
    }
}

void bflush(struct fastbuf *f)
{
  if (f->bptr > f->bstop)
    do_spout(f);
  else
    f->bptr = f->bstop; /* XXX: Skip the rest of the reading buffer ==> it breaks the position of the FE cursor */
  DBG("FB: flushed");
}

static void do_seek(struct fastbuf *f, ucw_off_t pos, int whence)
{
  bflush(f);
  DBG("FB: seeking to pos=%lld whence=%d %p %p %p %p", (long long)pos, whence, f->buffer, f->bstop, f->bptr, f->bufend);
  if (!f->seek || !f->seek(f, pos, whence))
    bthrow(f, "fb.seek", "Stream not seekable");
  DBG("FB: seeked %p %p %p %p", f->buffer, f->bstop, f->bptr, f->bufend);
  ASSERT(f->buffer <= f->bstop && f->bstop <= f->bptr && f->bptr <= f->bufend);
  if (whence == SEEK_SET)
    ASSERT(pos == btell(f));
  else
    ASSERT(btell(f) >= 0);
}

inline void bsetpos(struct fastbuf *f, ucw_off_t pos)
{
  /* We can optimize seeks only when reading */
  if (f->bptr < f->bstop && pos <= f->pos && pos >= f->pos - (f->bstop - f->buffer)) /* If bptr == bstop, then [buffer, bstop] may be undefined */
    f->bptr = f->bstop + (pos - f->pos);
  else if (pos != btell(f))
    {
      if (pos < 0)
	bthrow(f, "fb.seek", "Seek out of range");
      do_seek(f, pos, SEEK_SET);
    }
}

void bseek(struct fastbuf *f, ucw_off_t pos, int whence)
{
  switch (whence)
    {
    case SEEK_SET:
      bsetpos(f, pos);
      break;
    case SEEK_CUR:
      bsetpos(f, btell(f) + pos); /* btell() is non-negative, so an overflow will always throw "Seek out of range" in bsetpos() */
      break;
    case SEEK_END:
      if (pos > 0)
	bthrow(f, "fb.seek", "Seek out of range");
      do_seek(f, pos, SEEK_END);
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

void bbcopy_slow(struct fastbuf *f, struct fastbuf *t, uns l)
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
	  bthrow(f, "fb.read", "bbcopy: source exhausted");
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

int bconfig(struct fastbuf *f, uns item, int value)
{
  return (f->config && !(f->flags & FB_DEAD)) ? f->config(f, item, value) : -1;
}

void brewind(struct fastbuf *f)
{
  bflush(f);
  bsetpos(f, 0);
}

int bskip_slow(struct fastbuf *f, uns len)
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

ucw_off_t bfilesize(struct fastbuf *f)
{
  if (!f)
    return 0;
  if (!f->seek)
    return -1;
  ucw_off_t pos = btell(f);
  bflush(f);
  if (!f->seek(f, 0, SEEK_END))
    return -1;
  ucw_off_t len = btell(f);
  bsetpos(f, pos);
  return len;
}

/* Resources */

static void fb_res_detach(struct resource *r)
{
  struct fastbuf *f = r->priv;
  f->res = NULL;
}

static void fb_res_free(struct resource *r)
{
  struct fastbuf *f = r->priv;
  f->res = NULL;
  bclose(f);
}

static void fb_res_dump(struct resource *r, uns indent UNUSED)
{
  struct fastbuf *f = r->priv;
  printf(" name=%s\n", f->name);
}

static const struct res_class fb_res_class = {
  .name = "fastbuf",
  .detach = fb_res_detach,
  .dump = fb_res_dump,
  .free = fb_res_free,
};

struct fastbuf *fb_tie(struct fastbuf *f)
{
  f->res = res_new(&fb_res_class, f);
  return f;
}
