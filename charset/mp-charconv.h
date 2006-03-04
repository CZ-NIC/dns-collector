/*
 *	Sherlock Library -- Character Conversion with Allocation on a Memory Pool 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#ifndef _MP_CHARCONV_H
#define _MP_CHARCONV_H

#include "lib/mempool.h"
#include "charset/charconv.h"

byte *
mp_conv(struct mempool *mp, byte *s, uns cs_in, uns cs_out);

static inline byte *
mp_conv_to_utf8(struct mempool *mp, byte *s, uns cs_in) 
{ return mp_conv(mp, s, cs_in, CONV_CHARSET_UTF8); }

static inline byte *
mp_conv_from_utf8(struct mempool *mp, byte *s, uns cs_out)
{ return mp_conv(mp, s, CONV_CHARSET_UTF8, cs_out); }

#endif
