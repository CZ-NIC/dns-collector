/*
 *	Sherlock Library -- Block Hash Function
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/hashfunc.h"

uns
hash_block(byte *k, uns len)
{
  uns h = len;
  while (len--)
    h = h*37 + *k++;
  return h;
}
