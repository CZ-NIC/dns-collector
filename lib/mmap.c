/*
 *	Sherlock Library -- Mapping of Files
 *
 *	(c) 1999 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "lib.h"

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1L)
#endif

void *
mmap_file(byte *name, unsigned *len)
{
  int fd = open(name, O_RDONLY);
  struct stat st;
  void *x;

  if (fd < 0)
    return NULL;
  if (fstat(fd, &st) < 0)
    x = NULL;
  else
    {
      if (len)
	*len = st.st_size;
      x = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
      if (x == MAP_FAILED)
	x = NULL;
    }
  close(fd);
  return x;
}
