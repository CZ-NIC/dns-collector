/*
 *	Sherlock Library -- Binary Logarithm
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"

#undef log2

int
log2(u32 x)
{
  uns l;

  if (!x)
	return 0;

  l = 0;
  if (x & 0xffff0000) { l += 16; x &= 0xffff0000; }
  if (x & 0xff00ff00) { l += 8;  x &= 0xff00ff00; }
  if (x & 0xf0f0f0f0) { l += 4;  x &= 0xf0f0f0f0; }
  if (x & 0xcccccccc) { l += 2;  x &= 0xcccccccc; }
  if (x & 0xaaaaaaaa) l++;
  return l;
}
