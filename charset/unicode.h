/*
 *	The UniCode Library
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _UNICODE_H
#define _UNICODE_H

#include "lib/config.h"

extern byte *_U_cat[], *_U_sig[];
extern word *_U_upper[], *_U_lower[], *_U_unaccent[];

#define _C_UPPER 1			/* Upper-case letters */
#define _C_LOWER 2			/* Lower-case letters */
#define _C_PRINT 4			/* Printable */
#define _C_DIGIT 8			/* Digits */
#define _C_CTRL 16			/* Control characters */
#define _C_XDIGIT 32			/* Hexadecimal digits */
#define _C_BLANK 64			/* Blanks */
#define _C_INNER 128			/* `inner punctuation' -- underscore etc. */

#define _C_ALPHA (_C_UPPER | _C_LOWER)
#define _C_ALNUM (_C_ALPHA | _C_DIGIT)
#define _C_WORD (_C_ALNUM | _C_INNER)
#define _C_WSTART (_C_ALPHA | _C_INNER)

static inline uns Ucategory(word x)
{
  if (_U_cat[x >> 8U])
    return _U_cat[x >> 8U][x & 0xff];
  else
    return 0;
}

static inline word Utoupper(word x)
{
  word w = (_U_upper[x >> 8U]) ? _U_upper[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline word Utolower(word x)
{
  word w = (_U_lower[x >> 8U]) ? _U_lower[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline word Uunaccent(word x)
{
  word w = (_U_unaccent[x >> 8U]) ? _U_unaccent[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline byte Usig(word x)
{
  if (_U_sig[x >> 8U])
    return _U_sig[x >> 8U][x & 0xff] ? : 0xff;
  else
    return 0xff;
}

#define UCat(x,y) (Ucategory(x) & (y))

#define Uupper(x) UCat(x, _C_UPPER)
#define Ulower(x) UCat(x, _C_LOWER)
#define Ualpha(x) UCat(x, _C_ALPHA)
#define Ualnum(x) UCat(x, _C_ALNUM)
#define Uprint(x) !Uctrl(x)
#define Udigit(x) UCat(x, _C_DIGIT)
#define Uxdigit(x) UCat(x, _C_XDIGIT)
#define Uword(x) UCat(x, _C_WORD)
#define Ublank(x) UCat(x, _C_BLANK)
#define Uctrl(x) UCat(x, _C_CTRL)
#define Uspace(x) Ublank(x)

#define UNI_REPLACEMENT 0xfffc

#define PUT_UTF8(p,u) do {		\
  if (u < 0x80)				\
    *p++ = u;				\
  else if (u < 0x800)			\
    {					\
      *p++ = 0xc0 | (u >> 6);		\
      *p++ = 0x80 | (u & 0x3f);		\
    }					\
  else					\
    {					\
      *p++ = 0xe0 | (u >> 12);		\
      *p++ = 0x80 | ((u >> 6) & 0x3f);	\
      *p++ = 0x80 | (u & 0x3f);		\
    }					\
  } while(0)

#define IS_UTF8(c) ((c) >= 0xc0)

#define GET_UTF8_CHAR(p,u) do {		\
    if (*p >= 0xf0)			\
      {	/* Too large, use replacement char */	\
	p++;				\
	while ((*p & 0xc0) == 0x80)	\
	  p++;				\
	u = UNI_REPLACEMENT;		\
      }					\
    else if (*p >= 0xe0)		\
      {					\
	u = *p++ & 0x0f;		\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
      }					\
    else				\
      {					\
	u = *p++ & 0x1f;		\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
      }					\
  } while (0)				\

#define GET_UTF8(p,u)			\
    if (IS_UTF8(*p))			\
      GET_UTF8_CHAR(p,u);		\
    else				\
      u = *p++

#define UTF8_SPACE(u) ((u) < 0x80 ? 1 : (u) < 0x800 ? 2 : 3)

uns ucs2_to_utf8(byte *, word *);
uns utf8_to_ucs2(word *, byte *);
byte *static_ucs2_to_utf8(word *);
uns Ustrlen(word *);

#endif
