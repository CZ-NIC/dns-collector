/*
 *	UCW Library -- Fast Buffered I/O on Files
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
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

struct fb_file {
  struct fastbuf fb;
  int fd;				/* File descriptor */
  int is_temp_file;			/* 0=normal file, 1=temporary file, delete on close, -1=shared FD */
  int keep_back_buf;			/* Optimize for backwards reading */
  sh_off_t wpos;			/* Real file position */
  uns wlen;				/* Window size */
};
#define FB_FILE(f) ((struct fb_file *)(f)->is_fastbuf)
#define FB_BUFFER(f) (byte *)(FB_FILE(f) + 1)

static int
bfd_refill(struct fastbuf *f)
{
  struct fb_file *F = FB_FILE(f);
  byte *read_ptr = (f->buffer = FB_BUFFER(f));
  uns blen = f->bufend - f->buffer, back = F->keep_back_buf ? blen >> 2 : 0, read_len = blen;
  /* Forward or no seek */
  if (F->wpos <= f->pos)
    {
      sh_off_t diff = f->pos - F->wpos;
      if (diff > ((sh_off_t)blen << 2)) /* FIXME: Formula for long forward seeks */
        {
long_seek:
	  f->bptr = f->buffer + back;
	  f->bstop = f->buffer + blen;
	  goto seek;
	}
      if ((uns)diff < back) /* Reuse part of previous window (also F->wpos == f->pos) */
        {
	  uns keep = back - (uns)diff;
	  if (keep >= F->wlen)
	    back = diff + (keep = F->wlen);
	  else
	    memmove(f->buffer, f->buffer + F->wlen - keep, keep);
	  read_len -= keep;
	  read_ptr += keep;
	}
      else /* Short forward seek */
        {
	  uns skip = diff - back;
	  F->wpos += skip;
	  while (skip)
	    {
	      int l = read(F->fd, f->buffer, MIN(skip, blen));
	      if (unlikely(l <= 0))
		if (l < 0)
		  die("Error reading %s: %m", f->name);
		else
		  {
		    F->wpos -= skip;
		    goto eof;
		  }
	      skip -= l;
	    }
	}
      f->bptr = f->buffer + back;
      f->bstop = f->buffer + blen;
    }
  /* Backwards seek */
  else
    {
      sh_off_t diff = F->wpos - f->pos;
      if (diff > ((sh_off_t)blen << 1)) /* FIXME: Formula for long backwards seeks */
        {
	  if ((sh_off_t)back > f->pos)
	    back = f->pos;
	  goto long_seek;
	}
      if ((uns)diff <= F->wlen) /* Seek into previous window (for example brewind) */
        {
	  f->bstop = f->buffer + F->wlen;
	  f->bptr = f->bstop - diff;
	  f->pos = F->wpos;
	  return 1;
	}
      back *= 3;
      if ((sh_off_t)back > f->pos)
	back = f->pos;
      f->bptr = f->buffer + back;
      read_len = back + diff - F->wlen;
      if (F->wlen && read_len < blen) /* Reuse part of previous window */
        {
	  uns keep = MIN(F->wlen, blen - read_len);
	  memmove(f->buffer + read_len, f->buffer, keep);
	  f->bstop = f->buffer + read_len + keep;
	}
      else
	f->bstop = f->buffer + (read_len = blen);
seek:
      F->wpos = f->pos + (f->buffer - f->bptr);
      if (sh_seek(F->fd, F->wpos, SEEK_SET) < 0)
	die("Error seeking %s: %m", f->name);
    }
  do
    {
      int l = read(F->fd, read_ptr, read_len);
      if (unlikely(l < 0))
	die("Error reading %s: %m", f->name);
      if (!l)
	if (unlikely(read_ptr < f->bptr))
	  goto eof;
	else
	  break; /* Incomplete read because of EOF */
      read_ptr += l;
      read_len -= l;
      F->wpos += l;
    }
  while (read_ptr <= f->bptr);
  if (read_len)
    f->bstop = read_ptr;
  f->pos += f->bstop - f->bptr;
  F->wlen = f->bstop - f->buffer;
  return f->bstop - f->bptr;
eof:
  /* Seeked behind EOF */
  f->bptr = f->bstop = f->buffer;
  F->wlen = 0;
  return 0;
}

static void
bfd_spout(struct fastbuf *f)
{
  if (FB_FILE(f)->wpos != f->pos && sh_seek(FB_FILE(f)->fd, f->pos, SEEK_SET) < 0)
    die("Error seeking %s: %m", f->name);

  int l = f->bptr - f->buffer;
  byte *c = f->buffer;

  FB_FILE(f)->wpos = (f->pos += l);
  FB_FILE(f)->wlen = 0;
  while (l)
    {
      int z = write(FB_FILE(f)->fd, c, l);
      if (z <= 0)
	die("Error writing %s: %m", f->name);
      l -= z;
      c += z;
    }
  f->bptr = f->buffer = FB_BUFFER(f);
}

static int
bfd_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  sh_off_t l;
  switch (whence)
    {
      case SEEK_SET:
	f->pos = pos;
	return 1;
      case SEEK_CUR:
	l = f->pos + pos;
	if ((pos > 0) ^ (l > f->pos))
	  return 0;
	f->pos = l;
	return 1;
      case SEEK_END:
	l = sh_seek(FB_FILE(f)->fd, pos, SEEK_END);
	if (l < 0)
	  return 0;
	FB_FILE(f)->wpos = f->pos = l;
	FB_FILE(f)->wlen = 0;
	return 1;
      default:
	ASSERT(0);
    }
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
      if (close(FB_FILE(f)->fd))
	die("close(%s): %m", f->name);
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
      case BCONFIG_KEEP_BACK_BUF:
	FB_FILE(f)->keep_back_buf = value;
	return 0;
      default:
	return -1;
    }
}

struct fastbuf *
bfdopen_internal(int fd, byte *name, uns buflen)
{
  ASSERT(buflen);
  int namelen = strlen(name) + 1;
  struct fb_file *F = xmalloc_zero(sizeof(struct fb_file) + buflen + namelen);
  struct fastbuf *f = &F->fb;

  bzero(F, sizeof(*F));
  f->buffer = (byte *)(F+1);
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
  f->can_overwrite_buffer = 2;
  return f;
}

struct fastbuf *
bopen_try(byte *name, uns mode, uns buflen)
{
  return bopen_file_try(name, mode, &(struct fb_params){ .type = FB_STD, .buffer_size = buflen });
}

struct fastbuf *
bopen(byte *name, uns mode, uns buflen)
{
  return bopen_file(name, mode, &(struct fb_params){ .type = FB_STD, .buffer_size = buflen });
}

struct fastbuf *
bfdopen(int fd, uns buflen)
{
  return bopen_fd(fd, &(struct fb_params){ .type = FB_STD, .buffer_size = buflen });
}

struct fastbuf *
bfdopen_shared(int fd, uns buflen)
{
  struct fastbuf *f = bfdopen(fd, buflen);
  FB_FILE(f)->is_temp_file = -1;
  return f;
}

void
bfilesync(struct fastbuf *b)
{
  bflush(b);
  if (fsync(FB_FILE(b)->fd) < 0)
    log(L_ERROR, "fsync(%s) failed: %m", b->name);
}

#ifdef TEST

int main(int argc UNUSED, char **argv UNUSED)
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
