/*
 *	Sherlock Library -- Fast Buffered I/O on Files
 *
 *	(c) 1997--2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

struct fb_file {
  struct fastbuf fb;
  int fd;				/* File descriptor, -1 if not a real file */
  int is_temp_file;			/* 0=normal file, 1=temporary file, delete on close, -1=shared FD */
};
#define FB_FILE(f) ((struct fb_file *)(f)->is_fastbuf)

static int
bfd_refill(struct fastbuf *f)
{
  int l = read(FB_FILE(f)->fd, f->buffer, f->bufend-f->buffer);
  if (l < 0)
    die("Error reading %s: %m", f->name);
  f->bptr = f->buffer;
  f->bstop = f->buffer + l;
  f->pos += l;
  return l;
}

static void
bfd_spout(struct fastbuf *f)
{
  int l = f->bptr - f->buffer;
  char *c = f->buffer;

  f->pos += l;
  while (l)
    {
      int z = write(FB_FILE(f)->fd, c, l);
      if (z <= 0)
	die("Error writing %s: %m", f->name);
      l -= z;
      c += z;
    }
  f->bptr = f->buffer;
}

static void
bfd_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  sh_off_t l;

  if (whence == SEEK_SET && pos == f->pos)
    return;

  l = sh_seek(FB_FILE(f)->fd, pos, whence);
  if (l < 0)
    die("lseek on %s: %m", f->name);
  f->pos = l;
}

static void
bfd_close(struct fastbuf *f)
{
  switch (FB_FILE(f)->is_temp_file)
    {
    case 1:
      if (unlink(f->name) < 0)
	log(L_ERROR, "unlink(%s): %m", f->name);
    case 0:
      close(FB_FILE(f)->fd);
    }
  xfree(f);
}

static int
bfd_config(struct fastbuf *f, uns item, int value)
{
  switch (item)
    {
    case BCONFIG_IS_TEMP_FILE:
      FB_FILE(f)->is_temp_file = value;
      return 0;
    default:
      return -1;
    }
}

static struct fastbuf *
bfdopen_internal(int fd, uns buflen, byte *name)
{
  int namelen = strlen(name) + 1;
  struct fb_file *F = xmalloc(sizeof(struct fb_file) + buflen + namelen);
  struct fastbuf *f = &F->fb;

  bzero(F, sizeof(*F));
  f->buffer = (char *)(F+1);
  f->bptr = f->bstop = f->buffer;
  f->bufend = f->buffer + buflen;
  f->name = f->bufend;
  memcpy(f->name, name, namelen);
  F->fd = fd;
  f->refill = bfd_refill;
  f->spout = bfd_spout;
  f->seek = bfd_seek;
  f->close = bfd_close;
  f->config = bfd_config;
  return f;
}

struct fastbuf *
bopen(byte *name, uns mode, uns buffer)
{
  struct fastbuf *b;

#if 0 /* FIXME: Dirty hack for testing fb-mmap */
  if (buffer >= 65536 && (mode == O_RDONLY))
    {
      log(L_DEBUG, "bopen_mm hack: %s", name);
      if (mode & O_WRONLY)
	mode = (mode & ~O_WRONLY) | O_RDWR;
      return bopen_mm(name, mode);
    }
#endif

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

struct fastbuf *
bfdopen_shared(int fd, uns buffer)
{
  struct fastbuf *f = bfdopen(fd, buffer);
  FB_FILE(f)->is_temp_file = -1;
  return f;
}

#ifdef TEST

int main(int argc, char **argv)
{
  struct fastbuf *f, *t;

  f = bopen("/etc/profile", O_RDONLY, 16);
  t = bfdopen(1, 13);
  bbcopy(f, t, 100);
  printf("%d %d\n", (int)btell(f), (int)btell(t));
  bclose(f);
  bclose(t);
  return 0;
}

#endif
