/*
 *	Sherlock Library -- Character Conversion with Allocation on the Stack 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#ifndef _STK_CHARCONV_H
#define _STK_CHARCONV_H

#include "charset/charconv.h"
#include <alloca.h>

#define stk_conv(s, cs_in, cs_out) \
    ({ struct conv_context _c; uns _l=stk_conv_internal(&_c, (s), (cs_in), (cs_out)); \
    if (_l) { _c.dest=_c.dest_start=alloca(_l); _c.dest_end=_c.dest+_l; conv_run(&_c); } \
    _c.dest_start; })

#define stk_conv_to_utf8(s, cs_in) stk_conv(s, cs_in, CONV_CHARSET_UTF8)
#define stk_conv_from_utf8(s, cs_out) stk_conv(s, CONV_CHARSET_UTF8, cs_out)
    
uns stk_conv_internal(struct conv_context *, byte *, uns, uns);

#endif
