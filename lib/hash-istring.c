/*
 *	Sherlock Library -- Case-Insensitive String Hash Function
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/hashfunc.h"
#include "lib/chartype.h"

#include <string.h>

uns
hash_string_nocase(byte *k)
{
  uns h = strlen(k);
  while (*k)
    h = h*37 + Cupcase(*k++);
  return h;
}
