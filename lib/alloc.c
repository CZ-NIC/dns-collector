/*
 *	Sherlock Library -- Memory Allocation
 *
 *	(c) 2000 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#include <stdlib.h>
#include <string.h>

void *
xmalloc(uns size)
{
  void *x = malloc(size);
  if (!x)
    die("Cannot allocate %d bytes of memory", size);
  return x;
}

void *
xmalloc_zero(uns size)
{
  void *x = xmalloc(size);
  bzero(x, size);
  return x;
}
