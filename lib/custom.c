/*
 *	Sherlock: Custom Parts of Configuration
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/index.h"

#include <stdlib.h>

#if 0		/* Example */

void
custom_get_lm(struct card_attr *ca, byte *attr)
{
  if (attr)
    ca->lm = atol(attr);
  else
    ca->lm = 0;
}

byte *
custom_parse_lm(u32 *dest, byte *value, uns intval)
{
  if (value)
    return "LM is an integer, not a string";
  *dest = intval;
  return NULL;
}

#endif
