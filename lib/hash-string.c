/*
 *	Sherlock Library -- String Hash Function
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/hashfunc.h"

#include <string.h>

uns
hash_string(byte *k)
{
  uns h = strlen(k);
  while (*k)
    h = h*37 + *k++;
  return h;
}
