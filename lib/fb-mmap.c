/*
 *	Sherlock Library -- Fast Buffered I/O on Memory-Mapped Files
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *  FIXME:
 *  - problems with temp files
 *  - O_WRONLY ? (& generally processing of mode bits)
 */

#ifdef TEST
#define MMAP_WINDOW_SIZE 16*PAGE_SIZE
#define MMAP_EXTEND_SIZE 4*PAGE_SIZE
#else
#define MMAP_WINDOW_SIZE 256*PAGE_SIZE
#define MMAP_EXTEND_SIZE 256*PAGE_SIZE
#endif

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/lfs.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/user.h>

struct fb_mmap {
  struct fastbuf fb;
  int fd;
  int dummy;				/* FIXME: dirty hack for is_temp_file, remove */
  sh_off_t file_size;
  sh_off_t file_extend;
  sh_off_t window_pos;
  int is_writeable;
};
#define FB_MMAP(f) ((struct fb_mmap *)(f)->is_fastbuf)

static void
bfmm_map_window(struct fastbuf *f)
{
  struct fb_mmap *F = FB_MMAP(f);
  sh_off_t pos0 = f->pos & ~(sh_off_t)(PAGE_SIZE-1);
  int l = MIN(MMAP_WINDOW_SIZE, F->file_extend - pos0);
  uns ll = ALIGN(l, PAGE_SIZE);
  uns oll = ALIGN(f->bufend - f->buffer, PAGE_SIZE);
  int prot = F->is_writeable ? (PROT_READ | PROT_WRITE) : PROT_READ;

  DBG(" ... Mapping %x(%x)+%x(%x) len=%x extend=%x", (int)pos0, (int)f->pos, ll, l, (int)F->file_size, (int)F->file_extend);
  if (ll != oll)
    {
      munmap(f->buffer, oll);
      f->buffer = NULL;
    }
  if (!f->buffer)
    f->buffer = sh_mmap(NULL, ll, prot, MAP_SHARED, F->fd, pos0);
  else
    f->buffer = sh_mmap(f->buffer, ll, prot, MAP_SHARED | MAP_FIXED, F->fd, pos0);
  if (f->buffer == (byte *) MAP_FAILED)
    die("mmap(%s): %m", f->name);
  if (ll > PAGE_SIZE)
    madvise(f->buffer, ll, MADV_SEQUENTIAL);
  f->bufend = f->buffer + l;
  f->bptr = f->buffer + (f->pos - pos0);
  F->window_pos = f->pos;
}

static int
bfmm_refill(struct fastbuf *f)
{
  struct fb_mmap *F = FB_MMAP(f);

  DBG("Refill <- %p %p %p %p", f->buffer, f->bptr, f->bstop, f->bufend);
  if (f->pos >= F->file_size)
    return 0;
  if (f->bstop >= f->bufend)
    bfmm_map_window(f);
  if (F->window_pos + (f->bufend - f->buffer) > F->file_size)
    f->bstop = f->buffer + (F->file_size - F->window_pos);
  else
    f->bstop = f->bufend;
  f->pos = F->window_pos + (f->bstop - f->buffer);
  DBG(" -> %p %p %p(%x) %p", f->buffer, f->bptr, f->bstop, (int)f->pos, f->bufend);
  return 1;
}

static void
bfmm_spout(struct fastbuf *f)
{
  struct fb_mmap *F = FB_MMAP(f);
  sh_off_t end = f->pos + (f->bptr - f->bstop);

  DBG("Spout <- %p %p %p %p", f->buffer, f->bptr, f->bstop, f->bufend);
  if (end > F->file_size)
    F->file_size = end;
  if (f->bptr < f->bufend)
    return;
  f->pos = end;
  if (f->pos >= F->file_extend)
    {
      F->file_extend = ALIGN(F->file_extend + MMAP_EXTEND_SIZE, (sh_off_t)PAGE_SIZE);
      if (sh_ftruncate(F->fd, F->file_extend))
	die("ftruncate(%s): %m", f->name);
    }
  bfmm_map_window(f);
  f->bstop = f->bptr;
  DBG(" -> %p %p %p(%x) %p", f->buffer, f->bptr, f->bstop, (int)f->pos, f->bufend);
}

static void
bfmm_seek(struct fastbuf *f, sh_off_t pos, int whence)
{
  if (whence == SEEK_END)
    pos += FB_MMAP(f)->file_size;
  else
    ASSERT(whence == SEEK_SET);
  ASSERT(pos >= 0 && pos <= FB_MMAP(f)->file_size);
  f->pos = pos;
  f->bptr = f->bstop = f->bufend;	/* force refill/spout call */
  DBG("Seek -> %p %p %p(%x) %p", f->buffer, f->bptr, f->bstop, (int)f->pos, f->bufend);
}

static void
bfmm_close(struct fastbuf *f)
{
  struct fb_mmap *F = FB_MMAP(f);

  if (f->buffer)
    munmap(f->buffer, ALIGN(f->bufend-f->buffer, PAGE_SIZE));
  if (F->file_extend > F->file_size &&
      sh_ftruncate(F->fd, F->file_size))
    die("ftruncate(%s): %m", f->name);
  close(F->fd);
  xfree(f);
}

static struct fastbuf *
bfmmopen_internal(int fd, byte *name, uns mode)
{
  int namelen = strlen(name) + 1;
  struct fb_mmap *F = xmalloc(sizeof(struct fb_mmap) + namelen);
  struct fastbuf *f = &F->fb;

  bzero(F, sizeof(*F));
  f->name = (byte *)(F+1);
  memcpy(f->name, name, namelen);
  F->fd = fd;
  F->file_extend = F->file_size = sh_seek(fd, 0, SEEK_END);
  if (mode & O_APPEND)
    f->pos = F->file_size;
  mode &= ~O_APPEND;
  if (mode & O_WRONLY || mode & O_RDWR)
    F->is_writeable = 1;

  f->refill = bfmm_refill;
  f->spout = bfmm_spout;
  f->seek = bfmm_seek;
  f->close = bfmm_close;
  return f;
}

struct fastbuf *
bopen_mm(byte *name, uns mode)
{
  int fd = sh_open(name, mode, 0666);
  if (fd < 0)
    die("Unable to %s file %s: %m",
	(mode & O_CREAT) ? "create" : "open", name);
  return bfmmopen_internal(fd, name, mode);
}

#ifdef TEST

int main(int argc, char **argv)
{
  struct fastbuf *f = bopen_mm(argv[1], O_RDONLY);
  struct fastbuf *g = bopen_mm(argv[2], O_RDWR | O_CREAT | O_TRUNC);
  int c;

  DBG("Copying");
  while ((c = bgetc(f)) >= 0)
    bputc(g, c);
  bclose(f);
  DBG("Seek inside last block");
  bsetpos(g, btell(g)-1333);
  bputc(g, 13);
  DBG("Seek to the beginning & write");
  bsetpos(g, 1333);
  bputc(g, 13);
  DBG("flush");
  bflush(g);
  bputc(g, 13);
  bflush(g);
  DBG("Seek nearby & read");
  bsetpos(g, 133);
  bgetc(g);
  DBG("Seek far & read");
  bsetpos(g, 133333);
  bgetc(g);
  DBG("Closing");
  bclose(g);

  return 0;
}

#endif
