/*
 *	Sherlock Library -- Fast File Buffering
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib.h"
#include "fastbuf.h"

struct fastbuf *__bfdopen(int fd, uns buffer, byte *name)
{
  struct fastbuf *b = xmalloc(sizeof(struct fastbuf));

  b->buflen = buffer;
  b->buffer = xmalloc(buffer);
  b->bptr = b->bstop = b->buffer;
  b->bufend = b->buffer + buffer;
  b->name = stralloc(name);
  b->pos = b->fdpos = 0;
  b->fd = fd;
  return b;
}

struct fastbuf *
bopen(byte *name, uns mode, uns buffer)
{
  int fd = open(name, mode, 0666);

  if (fd < 0)
    die("Unable to %s file %s: %m",
	(mode & O_CREAT) ? "create" : "open", name);
  return __bfdopen(fd, buffer, name);
}

struct fastbuf *
bfdopen(int fd, uns buffer)
{
  byte x[32];

  sprintf(x, "fd%d", fd);
  return __bfdopen(fd, buffer, x);
}

void bclose(struct fastbuf *f)
{
  bflush(f);
  close(f->fd);
  free(f->name);
  free(f->buffer);
  free(f);
}

static int
rdbuf(struct fastbuf *f)
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
wrbuf(struct fastbuf *f)
{
  int l = f->bptr - f->buffer;

  if (l)
    {
      if (write(f->fd, f->buffer, l) != l)
	die("Error writing %s: %m", f->name);
      f->bptr = f->buffer;
      f->fdpos += l;
      f->pos = f->fdpos;
    }
}

void bflush(struct fastbuf *f)
{
  if (f->bptr != f->buffer)
    {					/* Have something to flush */
      if (f->bstop > f->buffer)		/* Read data? */
	{
	  f->bptr = f->bstop = f->buffer;
	  f->pos = f->fdpos;
	}
      else				/* Write data... */
	wrbuf(f);
    }
}

inline void bsetpos(struct fastbuf *f, uns pos)
{
  if (pos >= f->pos && (pos <= f->pos + (f->bptr - f->buffer) || pos <= f->pos + (f->bstop - f->buffer)))
    f->bptr = f->buffer + (pos - f->pos);
  else
    {
      bflush(f);
      if (f->fdpos != pos && lseek(f->fd, pos, SEEK_SET) < 0)
	die("lseek on %s: %m", f->name);
      f->fdpos = f->pos = pos;
    }
}

void bseek(struct fastbuf *f, uns pos, int whence)
{
  int l;

  switch (whence)
    {
    case SEEK_SET:
      return bsetpos(f, pos);
    case SEEK_CUR:
      return bsetpos(f, btell(f) + pos);
    case SEEK_END:
      bflush(f);
      l = lseek(f->fd, pos, whence);
      if (l < 0)
	die("lseek on %s: %m", f->name);
      f->fdpos = f->pos = l;
      break;
    default:
      die("bseek: invalid whence=%d", whence);
    }
}

int bgetc_slow(struct fastbuf *f)
{
  if (f->bptr < f->bstop)
    return *f->bptr++;
  if (!rdbuf(f))
    return EOF;
  return *f->bptr++;
}

int bpeekc_slow(struct fastbuf *f)
{
  if (f->bptr < f->bstop)
    return *f->bptr;
  if (!rdbuf(f))
    return EOF;
  return *f->bptr;
}

void bputc_slow(struct fastbuf *f, byte c)
{
  if (f->bptr >= f->bufend)
    wrbuf(f);
  *f->bptr++ = c;
}

word bgetw_slow(struct fastbuf *f)
{
  word w = bgetc_slow(f);
#ifdef CPU_BIG_ENDIAN
  return (w << 8) | bgetc_slow(f);
#else
  return w | (bgetc_slow(f) << 8);
#endif
}

ulg bgetl_slow(struct fastbuf *f)
{
  ulg l = bgetc_slow(f);
#ifdef CPU_BIG_ENDIAN
  l = (l << 8) | bgetc_slow(f);
  l = (l << 8) | bgetc_slow(f);
  return (l << 8) | bgetc_slow(f);
#else
  l = (bgetc_slow(f) << 8) | l;
  l = (bgetc_slow(f) << 16) | l;
  return (bgetc_slow(f) << 24) | l;
#endif
}

void bputw_slow(struct fastbuf *f, word w)
{
#ifdef CPU_BIG_ENDIAN
  bputc_slow(f, w >> 8);
  bputc_slow(f, w);
#else
  bputc_slow(f, w);
  bputc_slow(f, w >> 8);
#endif
}

void bputl_slow(struct fastbuf *f, ulg l)
{
#ifdef CPU_BIG_ENDIAN
  bputc_slow(f, l >> 24);
  bputc_slow(f, l >> 16);
  bputc_slow(f, l >> 8);
  bputc_slow(f, l);
#else
  bputc_slow(f, l);
  bputc_slow(f, l >> 8);
  bputc_slow(f, l >> 16);
  bputc_slow(f, l >> 24);
#endif
}

void bread_slow(struct fastbuf *f, void *b, uns l)
{
  while (l)
    {
      uns k = f->bstop - f->bptr;

      if (!k)
	{
	  rdbuf(f);
	  k = f->bstop - f->bptr;
	  if (!k)
	    die("bread on %s: file exhausted", f->name);
	}
      if (k > l)
	k = l;
      memcpy(b, f->bptr, k);
      f->bptr += k;
      b = (byte *)b + k;
      l -= k;
    }
}

void bwrite_slow(struct fastbuf *f, void *b, uns l)
{
  while (l)
    {
      uns k = f->bufend - f->bptr;

      if (!k)
	{
	  wrbuf(f);
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

byte *					/* Non-standard */
bgets(struct fastbuf *f, byte *b, uns l)
{
  byte *e = b + l - 1;
  int k;

  k = bgetc(f);
  if (k == EOF)
    return NULL;
  while (b < e)
    {
      if (k == '\n' || k == EOF)
	{
	  *b = 0;
	  return b;
	}
      *b++ = k;
      k = bgetc(f);
    }
  die("%s: Line too long", f->name);
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
      wrbuf(t);
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
	  wrbuf(t);
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
