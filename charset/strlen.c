/*
 *	The UniCode Library -- String Length
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include "charset/unicode.h"

uns
Ustrlen(word *w)
{
  word *z = w;

  while (*z)
    z++;
  return z - w;
}
