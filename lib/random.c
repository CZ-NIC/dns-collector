/*
 *	Sherlock Library -- Unbiased Range Correction for random()
 *
 *	(c) 1998 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#include <stdlib.h>

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
