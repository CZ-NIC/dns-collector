/*
 *	Sherlock Library -- Unbiased Range Correction for random()
 *
 *	(c) 1998 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib/lib.h"

uns
random_max(uns max)
{
  uns r, l;

  l = (RAND_MAX + 1U) - ((RAND_MAX + 1U) % max);
  do
    r = random();
  while (r >= l);
  return r % max;
}
