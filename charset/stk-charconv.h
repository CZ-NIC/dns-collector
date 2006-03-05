/*
 *	Sherlock Library -- Character Conversion with Allocation on the Stack 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#ifndef _CHARSET_STK_CHARCONV_H
#define _CHARSET_STK_CHARCONV_H

#include "charset/charconv.h"
#include <alloca.h>

/* The following macros convert strings between given charsets (CONV_CHARSET_x). */

#define stk_conv(s, cs_in, cs_out) \
    ({ struct stk_conv_context _c; stk_conv_init(&_c, (s), (cs_in), (cs_out)); \
       while (stk_conv_step(&_c, alloca(_c.request))); _c.c.dest_start; })

#define stk_conv_to_utf8(s, cs_in) stk_conv(s, cs_in, CONV_CHARSET_UTF8)
#define stk_conv_from_utf8(s, cs_out) stk_conv(s, CONV_CHARSET_UTF8, cs_out)
   
/* Internal structure and routines. */

struct stk_conv_context {
  struct conv_context c;
  uns count;
  uns sum;
  uns request;
  byte *buf[16]; 
  uns size[16];
};

void stk_conv_init(struct stk_conv_context *c, byte *s, uns cs_in, uns cs_out);
int stk_conv_step(struct stk_conv_context *c, byte *buf);

#endif
