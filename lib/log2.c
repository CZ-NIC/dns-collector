/*
 *	Sherlock Library -- Binary Logarithm
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>

#include "lib.h"

int
ffs(ulg x)
{
  ulg l;

  if (!x)
	return 0;

  l = 0;
  if (x & 0xffff0000) l += 16;
  if (x & 0xff00ff00) l += 8;
  if (x & 0xf0f0f0f0) l += 4;
  if (x & 0xcccccccc) l += 2;
  if (x & 0xaaaaaaaa) l++;
  return l;
}
