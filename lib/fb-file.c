/*
 *	Sherlock Library -- Fast Buffered I/O on Files
 *
 *	(c) 1997--2000 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int
bfd_refill(struct fastbuf *f)
{
  int l = read(f->fd, f->buffer, f->buflen);

  if (l < 0)
    die("Error reading %s: %m", f->name);
  f->bptr = f->buffer;
  f->bstop = f->buffer + l;
  f->pos = f->fdpos;
  f->fdpos += l;
  return l;
}

static void
bfd_spout(struct fastbuf *f)
{
  int l = f->bptr - f->buffer;
  char *c = f->buffer;

  while (l)
    {
      int z = write(f->fd, c, l);
      if (z <= 0)
	die("Error writing %s: %m", f->name);
      f->fdpos += z;
      l -= z;
      c += z;
    }
  f->bptr = f->buffer;
  f->pos = f->fdpos;
}

static void
bfd_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  sh_off_t l;

  if (whence == SEEK_SET && pos == f->fdpos)
    return;

  l = sh_seek(f->fd, pos, whence);
  if (l < 0)
    die("lseek on %s: %m", f->name);
  f->fdpos = f->pos = l;
}

static void
bfd_close(struct fastbuf *f)
{
  close(f->fd);
  if (f->is_temp_file && unlink(f->name) < 0)
    die("unlink(%s): %m", f->name);
}

static struct fastbuf *
bfdopen_internal(int fd, uns buflen, byte *name)
{
  int namelen = strlen(name) + 1;
  struct fastbuf *b = xmalloc_zero(sizeof(struct fastbuf) + buflen + namelen);

  b->buflen = buflen;
  b->buffer = (char *)(b+1);
  b->bptr = b->bstop = b->buffer;
  b->bufend = b->buffer + buflen;
  b->name = b->bufend;
  strcpy(b->name, name);
  b->fd = fd;
  b->refill = bfd_refill;
  b->spout = bfd_spout;
  b->seek = bfd_seek;
  b->close = bfd_close;
  return b;
}

struct fastbuf *
bopen(byte *name, uns mode, uns buffer)
{
  struct fastbuf *b;
  int fd = sh_open(name, mode, 0666);
  if (fd < 0)
    die("Unable to %s file %s: %m",
	(mode & O_CREAT) ? "create" : "open", name);
  b = bfdopen_internal(fd, buffer, name);
  if (mode & O_APPEND)
    bfd_seek(b, 0, SEEK_END);
  return b;
}

struct fastbuf *
bfdopen(int fd, uns buffer)
{
  byte x[32];

  sprintf(x, "fd%d", fd);
  return bfdopen_internal(fd, buffer, x);
}

void bbcopy(struct fastbuf *f, struct fastbuf *t, uns l)
{
  uns rf = f->bstop - f->bptr;

  if (!l)
    return;
  if (rf)
    {
      uns k = (rf <= l) ? rf : l;
      bwrite(t, f->bptr, k);
      f->bptr += k;
      l -= k;
    }
  while (l >= t->buflen)
    {
      t->spout(t);
      if ((uns) read(f->fd, t->buffer, t->buflen) != t->buflen)
	die("bbcopy: %s exhausted", f->name);
      f->pos = f->fdpos;
      f->fdpos += t->buflen;
      f->bstop = f->bptr = f->buffer;
      t->bptr = t->bufend;
      l -= t->buflen;
    }
  while (l)
    {
      uns k = t->bufend - t->bptr;

      if (!k)
	{
	  t->spout(t);
	  k = t->bufend - t->bptr;
	}
      if (k > l)
	k = l;
      bread(f, t->bptr, k);
      t->bptr += k;
      l -= k;
    }
}

#ifdef TEST

int main(int argc, char **argv)
{
  struct fastbuf *f, *t;
  int c;

  f = bopen("/etc/profile", O_RDONLY, 16);
  t = bfdopen(1, 13);
  bbcopy(f, t, 100);
  bclose(f);
  bclose(t);
}

#endif
