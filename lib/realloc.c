/*
 *	Sherlock Library -- Memory Re-allocation
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib.h"

void *
xrealloc(void *old, uns size)
{
  void *x = realloc(old, size);
  if (!x)
    die("Cannot reallocate %d bytes of memory", size);
  return x;
}
