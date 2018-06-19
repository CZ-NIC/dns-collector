/*
 *	UCW Library -- Character Conversion with Allocation on a Memory Pool
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _CHARSET_MP_CHARCONV_H
#define _CHARSET_MP_CHARCONV_H

#include <ucw/mempool.h>
#include <charset/charconv.h>

#ifdef CONFIG_UCW_CLEAN_ABI
#define mp_strconv ucw_mp_strconv
#endif

byte *mp_strconv(struct mempool *mp, const byte *s, uint cs_in, uint cs_out);

static inline byte *mp_strconv_to_utf8(struct mempool *mp, const byte *s, uint cs_in)
{ return mp_strconv(mp, s, cs_in, CONV_CHARSET_UTF8); }

static inline byte *mp_strconv_from_utf8(struct mempool *mp, const byte *s, uint cs_out)
{ return mp_strconv(mp, s, CONV_CHARSET_UTF8, cs_out); }

#endif
