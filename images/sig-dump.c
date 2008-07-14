/*
 *	Image Library -- Dumping of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "images/images.h"
#include "images/signature.h"
#include <stdio.h>

byte *
image_vector_dump(byte *buf, struct image_vector *vec)
{
  byte *p = buf;
  *p++ = '(';
  for (uns i = 0; i < IMAGE_VEC_F; i++)
    {
      if (i)
	*p++ = ' ';
      p += sprintf(p, "%u", vec->f[i]);
    }
  *p++ = ')';
  *p = 0;
  return buf;
}

byte *
image_region_dump(byte *buf, struct image_region *reg)
{
  byte *p = buf;
  p += sprintf(p, "(txt=");
  for (uns i = 0; i < IMAGE_REG_F; i++)
    {
      if (i)
	*p++ = ' ';
      p += sprintf(p, "%u", reg->f[i]);
    }
  p += sprintf(p, " shp=");
  for (uns i = 0; i < IMAGE_REG_H; i++)
    {
      if (i)
	*p++ = ' ';
      p += sprintf(p, "%u", reg->h[i]);
    }
  p += sprintf(p, " wa=%u wb=%u)", reg->wa, reg->wb);
  *p = 0;
  return buf;
}
