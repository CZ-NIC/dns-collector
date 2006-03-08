/*
 *	Sherlock Library -- Character Conversion with Allocation on the Stack 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _CHARSET_STK_CHARCONV_H
#define _CHARSET_STK_CHARCONV_H

#include "charset/charconv.h"
#include <alloca.h>

/* The following macros convert strings between given charsets (CONV_CHARSET_x). */

#define stk_conv(s, cs_in, cs_out) \
    ({ struct conv_context _c; uns _l=stk_conv_init(&_c, (s), (cs_in), (cs_out)); \
       while (_l) _l=stk_conv_step(&_c, alloca(_l), _l); _c.dest_start; })

#define stk_conv_to_utf8(s, cs_in) stk_conv(s, cs_in, CONV_CHARSET_UTF8)
#define stk_conv_from_utf8(s, cs_out) stk_conv(s, CONV_CHARSET_UTF8, cs_out)
   
/* Internals */

uns stk_conv_init(struct conv_context *c, byte *s, uns cs_in, uns cs_out);
uns stk_conv_step(struct conv_context *c, byte *buf, uns len);

#endif
