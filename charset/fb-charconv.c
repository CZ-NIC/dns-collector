/*
 *	Sherlock Library -- Charset Conversion Wrapper for Fast Buffered I/O
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "charset/charconv.h"
#include "charset/fb-charconv.h"

#include <stdlib.h>

#define BUFSIZE 1024

struct fb_charconv {
  struct fastbuf fb;
  struct fastbuf *out;
  struct conv_context ctxt;
  byte buf[BUFSIZE];
};
#define FB_CC(f) ((struct fb_charconv *)(f)->is_fastbuf)

static void
fb_cc_spout(struct fastbuf *f)
{
  struct conv_context *ct = &FB_CC(f)->ctxt;
  int flags;

  ct->source = f->buffer;
  ct->source_end = f->bptr;
  do
    {
      flags = conv_run(ct);
      if (ct->dest > ct->dest_start)
	bdirect_write_commit(FB_CC(f)->out, ct->dest);
      uns l = bdirect_write_prepare(FB_CC(f)->out, &ct->dest_start);
      ct->dest = ct->dest_start;
      ct->dest_end = ct->dest + l;
    }
  while (!(flags & CONV_SOURCE_END));

  f->bptr = f->buffer;
}

static void
fb_cc_close(struct fastbuf *f)
{
  bflush(FB_CC(f)->out);
  xfree(f);
}

struct fastbuf *
fb_wrap_charconv_out(struct fastbuf *f, int cs_from, int cs_to)
{
  if (cs_from == cs_to)
    return f;

  struct fastbuf *g = xmalloc_zero(sizeof(struct fb_charconv));
  FB_CC(g)->out = f;
  conv_init(&FB_CC(g)->ctxt);
  conv_set_charset(&FB_CC(g)->ctxt, cs_from, cs_to);
  g->name = "<charconv-out>";
  g->spout = fb_cc_spout;
  g->close = fb_cc_close;
  g->buffer = g->bstop = g->bptr = FB_CC(g)->buf;
  g->bufend = g->buffer + BUFSIZE;
  return g;
}
