/*
 *	Sherlock Library -- Mapping of File Parts
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/lfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

struct partmap {
  int fd;
  sh_off_t file_size;
  sh_off_t start_off, end_off;
  byte *start_map;
  int writeable;
};

#ifdef TEST
#define PARTMAP_WINDOW 4096
#else
#define PARTMAP_WINDOW 16777216
#endif

struct partmap *
partmap_open(byte *name, int writeable)
{
  struct partmap *p = xmalloc_zero(sizeof(struct partmap));

  p->fd = sh_open(name, writeable ? O_RDWR : O_RDONLY);
  if (p->fd < 0)
    die("open(%s): %m", name);
  if ((p->file_size = sh_seek(p->fd, 0, SEEK_END)) < 0)
    die("lseek(%s): %m", name);
  p->writeable = writeable;
  return p;
}

sh_off_t
partmap_size(struct partmap *p)
{
  return p->file_size;
}

void
partmap_close(struct partmap *p)
{
  if (p->start_map)
    munmap(p->start_map, p->end_off - p->start_off);
  close(p->fd);
  xfree(p);
}

void *
partmap_map(struct partmap *p, sh_off_t start, uns size)
{
  if (!p->start_map || start < p->start_off || start+size > p->end_off)
    {
      if (p->start_map)
	munmap(p->start_map, p->end_off - p->start_off);
      uns win = PARTMAP_WINDOW;
      ASSERT(win >= size);
      if (start+win > p->file_size)
	win = p->file_size - start;
      p->start_map = sh_mmap(NULL, win, p->writeable ? (PROT_READ | PROT_WRITE) : PROT_READ, MAP_SHARED, p->fd, start);
      if (p->start_map == MAP_FAILED)
	die("mmap failed at position %Ld: %m", (long long)start);
      p->start_off = start;
      p->end_off = start+win;
    }
  return p->start_map + (start - p->start_off);
}

#ifdef TEST
int main(int argc, char **argv)
{
  struct partmap *p = partmap_open(argv[1], 0);
  uns l = partmap_size(p);
  uns i;
  for (i=0; i<l; i++)
    putchar(*(char *)partmap_map(p, i, 1));
  partmap_close(p);
  return 0;
}
#endif
