/*
 *	Sherlock: Custom Parts of Configuration
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/index.h"

#include <stdlib.h>

#ifdef CONFIG_IMAGES

void
custom_create_attrs(struct odes *odes, struct card_attr *ca)
{
  byte *x;
  uns ox, oy, tw, th, ncolors;
  uns ifmt, isize, icol;
  byte ocspace[10];
  byte *ctype;

  x = obj_find_aval(odes, 'G');
  ctype = obj_find_aval(odes, 'T');
  if (!x || !ctype || sscanf(x, "%d%d%s%d%d%d", &ox, &oy, ocspace, &ncolors, &tw, &th) != 6)
    {
      ca->image_flags = 0;
      return;
    }

  if (!strcasecmp(ctype, "image/jpeg"))
    ifmt = 1;
  else if (!strcasecmp(ctype, "image/png"))
    ifmt = 2;
  else if (!strcasecmp(ctype, "image/gif"))
    ifmt = 3;
  else
    {
      log(L_ERROR, "Unknown image content-type: %s", ctype);
      ifmt = 0;
    }

  if (ox <= 100 && oy <= 100)
    isize = 0;
  else if (ox <= 320 && oy <= 200)
    isize = 1;
  else if (ox <= 640 && oy <= 480)
    isize = 2;
  else
    isize = 3;

  if (!strcasecmp(ocspace, "GRAY"))
    icol = 0;
  else if (ncolors <= 16)
    icol = 1;
  else if (ncolors <= 256)
    icol = 2;
  else
    icol = 3;

  ca->image_flags = ifmt | (isize << 2) | (icol << 4);
}

byte *
custom_it_parse(u32 *dest, byte *value, uns intval)
{
  if (value)
    return "IMGTYPE: number expected";
  if (intval > 3)
    return "IMGTYPE out of range";
  *dest = intval;
  return NULL;
}

byte *
custom_is_parse(u32 *dest, byte *value, uns intval)
{
  if (value)
    return "IMGSIZE: number expected";
  if (intval > 3)
    return "IMGSIZE out of range";
  *dest = intval;
  return NULL;
}

byte *
custom_ic_parse(u32 *dest, byte *value, uns intval)
{
  if (value)
    return "IMGCOLORS: number expected";
  if (intval > 3)
    return "IMGCOLORS out of range";
  *dest = intval;
  return NULL;
}

#endif

#if 0		/* Example */

/* FIXME: The example is wrong */

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
