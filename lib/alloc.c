/*
 *	Sherlock Library -- Memory Allocation
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib.h"

void *
xmalloc(uns size)
{
  void *x = malloc(size);
  if (!x)
	die("Cannot allocate %d bytes of memory", size);
  return x;
}
