/*
 *	Sherlock Library -- String Allocation
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <string.h>

#include "lib.h"

byte *
stralloc(byte *s)
{
  uns l = strlen(s);
  byte *k = xmalloc(l + 1);
  strcpy(k, s);
  return k;
}
