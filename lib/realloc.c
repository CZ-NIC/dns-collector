/*
 *	Sherlock Library -- Memory Re-allocation
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdlib.h>

#ifndef DMALLOC

void *
xrealloc(void *old, uns size)
{
  void *x = realloc(old, size);
  if (!x)
    die("Cannot reallocate %d bytes of memory", size);
  return x;
}

#endif
