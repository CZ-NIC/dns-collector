/*
 *	Sherlock Library -- Fast Buffered Input on Limited File Descriptors
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <stdlib.h>
#include <unistd.h>

struct fb_limfd {
  struct fastbuf fb;
  int fd;				/* File descriptor */
  int limit;
  int can_overwrite;
};
#define FB_LIMFD(f) ((struct fb_limfd *)(f)->is_fastbuf)

static int
bfl_refill(struct fastbuf *f)
{
  int max = MIN(FB_LIMFD(f)->limit - f->pos, f->bufend - f->buffer);
  int l = read(FB_LIMFD(f)->fd, f->buffer, max);
  if (l < 0)
    die("Error reading %s: %m", f->name);
  f->bptr = f->buffer;
  f->bstop = f->buffer + l;
  f->pos += l;
  return l;
}

static void
bfl_close(struct fastbuf *f)
{
  xfree(f);
}

static int
bfl_config(struct fastbuf *f, uns item, int value)
{
  switch (item)
    {
    case BCONFIG_CAN_OVERWRITE: ;
      int old_value = FB_LIMFD(f)->can_overwrite;
      if (value >= 0 && value <= 2)
	FB_LIMFD(f)->can_overwrite = value;
      return old_value;
    default:
      return -1;
    }
}

struct fastbuf *
bopen_limited_fd(int fd, uns buflen, uns limit)
{
  struct fb_limfd *F = xmalloc(sizeof(struct fb_limfd) + buflen);
  struct fastbuf *f = &F->fb;

  bzero(F, sizeof(*F));
  f->buffer = (char *)(F+1);
  f->bptr = f->bstop = f->buffer;
  f->bufend = f->buffer + buflen;
  f->name = "limited-fd";
  F->fd = fd;
  F->limit = limit;
  f->refill = bfl_refill;
  f->close = bfl_close;
  f->config = bfl_config;
  F->can_overwrite = 2;
  return f;
}

#ifdef TEST

int main(int argc, char **argv)
{
  struct fastbuf *f = bopen_limited_fd(0, 3, 13);
  struct fastbuf *o = bfdopen_shared(1, 16);
  int c;
  while ((c = bgetc(f)) >= 0)
    bputc(o, c);
  bclose(o);
  bclose(f);
  return 0;
}

#endif
