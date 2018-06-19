/*
 *	UCW Library -- Fast Buffered I/O on Static Buffers
 *
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>

#include <stdio.h>
#include <stdlib.h>

static int
fbbuf_refill(struct fastbuf *f)
{
  f->bstop = f->bufend;
  f->pos = f->bstop - f->buffer;
  return f->bptr < f->bstop;
}

static int
fbbuf_seek(struct fastbuf *f, ucw_off_t pos, int whence)
{
  /* Somebody might want to seek to the end of buffer, try to be nice to him. */
  ucw_off_t len = f->bufend - f->buffer;
  if (whence == SEEK_END)
    pos += len;
  if (pos < 0 || pos > len)
    bthrow(f, "seek", "Seek out of range");
  f->bstop = f->bptr = f->buffer + pos;
  f->pos = pos;
  return 1;
}

void
fbbuf_init_read(struct fastbuf *f, byte *buf, uint size, uint can_overwrite)
{
  *f = (struct fastbuf) {
    .buffer = buf,
    .bptr = buf,
    .bstop = buf + size,
    .bufend = buf + size,
    .name = "fbbuf-read",
    .pos = size,
    .refill = fbbuf_refill,
    .seek = fbbuf_seek,
    .can_overwrite_buffer = can_overwrite
  };
}

static void
fbbuf_spout(struct fastbuf *f)
{
  if (f->bptr >= f->bufend)
    bthrow(f, "write", "fbbuf: buffer overflow on write");
}

void
fbbuf_init_write(struct fastbuf *f, byte *buf, uint size)
{
  *f = (struct fastbuf) {
    .buffer = buf,
    .bstop = buf,
    .bptr = buf,
    .bufend = buf + size,
    .name = "fbbuf-write",
    .spout = fbbuf_spout,
  };
}

#ifdef TEST

int main(int argc, char *argv[])
{
  if (argc < 2)
    {
      fprintf(stderr, "You must specify a test (r, w, o)\n");
      return 1;
    }
  switch (*argv[1])
    {
      case 'r':
        {
          struct fastbuf fb;
          char *data = "Two\nlines\n";
          fbbuf_init_read(&fb, data, strlen(data), 0);
          char buffer[10];
          while (bgets(&fb, buffer, 10))
            puts(buffer);
          bclose(&fb);
          break;
        }
      case 'w':
        {
          struct fastbuf fb;
          char buff[20];
          fbbuf_init_write(&fb, buff, 20);
          bputs(&fb, "Hello world\n");
          bputc(&fb, 0);
          fputs(buff, stdout);
          break;
        }
      case 'o':
        {
          struct fastbuf fb;
          char buff[4];
          fbbuf_init_write(&fb, buff, 4);
          bputs(&fb, "Hello");
          bputc(&fb, 0);
          fputs(buff, stdout);
          break;
        }
    }
  return 0;
}

#endif
