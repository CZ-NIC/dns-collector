/*
 *	UCW Library -- Memory Allocation
 *
 *	(c) 2000 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdlib.h>
#include <string.h>

#ifndef DEBUG_DMALLOC

void *
xmalloc(uns size)
{
  void *x = malloc(size);
  if (!x)
    die("Cannot allocate %d bytes of memory", size);
  return x;
}

#endif

void *
xmalloc_zero(uns size)
{
  void *x = xmalloc(size);
  bzero(x, size);
  return x;
}
