/*
 *	Image Library -- Computation of image signatures
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "sherlock/sherlock.h"
#include "lib/fastbuf.h"
#include "images/images.h"
#include "images/signature.h"

uns
image_signature_read(struct fastbuf *fb, struct image_signature *sig)
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
image_signature_write(struct fastbuf *fb, struct image_signature *sig)
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
