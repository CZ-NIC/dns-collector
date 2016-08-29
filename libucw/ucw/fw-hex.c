/*
 *	UCW Library -- I/O Wrapper for Hexadecimal Escaped Debugging Output
 *
 *	(c) 2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/chartype.h>
#include <ucw/fastbuf.h>
#include <ucw/fw-hex.h>

#define HEX_BUFSIZE 1024

struct fb_hex {
  struct fastbuf fb;
  struct fastbuf *orig;
  byte buf[HEX_BUFSIZE];
};
#define FB_HEX(f) ((struct fb_hex *)(f))

static void
fb_hex_spout(struct fastbuf *f)
{
  struct fastbuf *orig = FB_HEX(f)->orig;

  for (byte *p = f->buffer; p < f->bptr; p++)
    {
      uint c = *p;
      if (c >= 0x21 && c <= 0x7e && c != '<' && c != '>')
	bputc(orig, c);
      else
	bprintf(orig, "<%02x>", c);
    }
  f->bptr = f->buffer;
}

static int
fb_hex_refill(struct fastbuf *f)
{
  struct fastbuf *orig = FB_HEX(f)->orig;

  f->bptr = f->bstop = f->buffer;
  while (f->bstop < f->bufend)
    {
      int c = bgetc(orig);
      if (c < 0)
	break;
      if (c == '<')
	{
	  uint x = 0;
	  for (int i=0; i<2; i++)
	    {
	      int d = bgetc(orig);
	      if (!Cxdigit(d))
		bthrow(f, "fbhex", "fb_hex: Malformed hexadecimal representation");
	      x = (x << 4) | Cxvalue(d);
	    }
	  c = bgetc(orig);
	  if (c != '>')
	    bthrow(f, "fbhex", "fb_hex: Expecting '>'");
	  *f->bstop++ = x;
	}
      else
	*f->bstop++ = c;
    }
  return (f->bstop > f->bptr);
}

static void
fb_hex_close(struct fastbuf *f)
{
  if (f->spout)
    bputc(FB_HEX(f)->orig, '\n');
  bflush(FB_HEX(f)->orig);
  xfree(f);
}

struct fastbuf *fb_wrap_hex_out(struct fastbuf *f)
{
  struct fastbuf *g = xmalloc_zero(sizeof(struct fb_hex));
  FB_HEX(g)->orig = f;
  g->name = "<hex-out>";
  g->spout = fb_hex_spout;
  g->close = fb_hex_close;
  g->buffer = g->bstop = g->bptr = FB_HEX(g)->buf;
  g->bufend = g->buffer + HEX_BUFSIZE;
  return g;
}

struct fastbuf *fb_wrap_hex_in(struct fastbuf *f)
{
  struct fastbuf *g = xmalloc_zero(sizeof(struct fb_hex));
  FB_HEX(g)->orig = f;
  g->name = "<hex-in>";
  g->refill = fb_hex_refill;
  g->close = fb_hex_close;
  g->buffer = g->bstop = g->bptr = FB_HEX(g)->buf;
  g->bufend = g->buffer + HEX_BUFSIZE;
  return g;
}
