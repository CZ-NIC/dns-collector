/*
 *	Image Library -- Dumping of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "lib/unaligned.h"
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

uns
get_image_signature(byte *buf, struct image_signature *sig)
{
  uns size = image_signature_size(*buf);
  memcpy(sig, buf, size);
#ifndef CPU_ALLOW_UNALIGNED
#define FIX_U16(x) PUT_U16(&(x), x)
  FIX_U16(sig->dh);
  struct image_region *reg = sig->reg;
  for (uns i = 0; i < sig->len; i++, reg++)
    {
      for (uns j = 0; j < IMAGE_REG_H; j++)
	FIX_U16(reg->h[j]);
      FIX_U16(reg->wa);
      FIX_U16(reg->wb);
    }
#undef FIX_U16  
#endif
  return size;
}

uns
put_image_signature(byte *buf, struct image_signature *sig)
{
  uns size = image_signature_size(sig->len);
  memcpy(buf, sig, size);
#ifndef CPU_ALLOW_UNALIGNED
#define FIX_U16(x) do { x = GET_U16(&(x)); } while(0)
  struct image_signature *tmp = (struct image_signature *)buf;
  FIX_U16(tmp->dh);
  struct image_region *reg = tmp->reg;
  for (uns i = 0; i < tmp->len; i++, reg++)
    {
      for (uns j = 0; j < IMAGE_REG_H; j++)
	FIX_U16(reg->h[j]);
      FIX_U16(reg->wa);
      FIX_U16(reg->wb);
    }
#undef FIX_U16
#endif
  return size;
}
uns
bget_image_signature(struct fastbuf *fb, struct image_signature *sig)
{
  uns size = image_signature_size(bpeekc(sig));
  breadb(fb, sig, size);
#ifndef CPU_ALLOW_UNALIGNED
#define FIX_U16(x) PUT_U16(&(x), x)
  FIX_U16(sig->dh);
  struct image_region *reg = sig->reg;
  for (uns i = 0; i < sig->len; i++, reg++)
    {
      for (uns j = 0; j < IMAGE_REG_H; j++)
	FIX_U16(reg->h[j]);
      FIX_U16(reg->wa);
      FIX_U16(reg->wb);
    }
#undef FIX_U16  
#endif
  return size;
}

uns
bput_image_signature(struct fastbuf *fb, struct image_signature *sig)
{
  uns size = image_signature_size(sig->len);
#ifdef CPU_ALLOW_UNALIGNED
  bwrite(fb, sig, size);
#else
  struct image_signature tmp;
  memcpy(tmp, sig, size);
#define FIX_U16(x) do { x = GET_U16(&(x)); } while(0)
  FIX_U16(tmp.dh);
  struct image_region *reg = tmp.reg;
  for (uns i = 0; i < tmp.len; i++, reg++)
    {
      for (uns j = 0; j < IMAGE_REG_H; j++)
	FIX_U16(reg->h[j]);
      FIX_U16(reg->wa);
      FIX_U16(reg->wb);
    }
  bwrite(fb, &tmp, size);
#undef FIX_U16
#endif
  return size;
}
