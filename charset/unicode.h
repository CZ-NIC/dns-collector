/*
 *	The UniCode Library
 *
 *	(c) 1997--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UNICODE_H
#define _UNICODE_H

extern const byte *_U_cat[];
extern const word *_U_upper[], *_U_lower[], *_U_unaccent[];

static inline uns Ucategory(uns x)
{
  if (_U_cat[x >> 8U])
    return _U_cat[x >> 8U][x & 0xff];
  else
    return 0;
}

static inline uns Utoupper(uns x)
{
  word w = (_U_upper[x >> 8U]) ? _U_upper[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline uns Utolower(uns x)
{
  word w = (_U_lower[x >> 8U]) ? _U_lower[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline uns Uunaccent(uns x)
{
  word w = (_U_unaccent[x >> 8U]) ? _U_unaccent[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

extern const word *Uexpand_lig(uns x);

enum unicode_char_type {
  _U_LETTER,			/* Letters */
  _U_UPPER,			/* Upper-case letters */
  _U_LOWER,			/* Lower-case letters */
  _U_CTRL,			/* Control characters */
  _U_DIGIT,			/* Digits */
  _U_XDIGIT,			/* Hexadecimal digits */
  _U_SPACE,			/* White spaces (spaces, tabs, newlines) */
  _U_LIGATURE,			/* Compatibility ligature (to be expanded) */
};

#define _U_LUPPER (_U_LETTER | _U_UPPER)
#define _U_LLOWER (_U_LETTER | _U_LOWER)

#define UCat(x,y) (Ucategory(x) & (y))

#define Ualpha(x) UCat(x, _U_LETTER)
#define Uupper(x) UCat(x, _U_UPPER)
#define Ulower(x) UCat(x, _U_LOWER)
#define Udigit(x) UCat(x, _U_DIGIT)
#define Uxdigit(x) UCat(x, (_U_DIGIT | _U_XDIGIT))
#define Ualnum(x) UCat(x, (_U_LETTER | _U_DIGIT))
#define Uctrl(x) UCat(x, _U_CTRL)
#define Uprint(x) !Uctrl(x)
#define Uspace(x) UCat(x, _U_SPACE)

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

#define UTF8_SKIP(p) do {				\
    uns c = *p++;					\
    if (c >= 0xc0)					\
      while (c & 0x40 && *p >= 0x80 && *p < 0xc0)	\
        p++, c <<= 1;					\
  } while (0)

#define UTF8_SPACE(u) ((u) < 0x80 ? 1 : (u) < 0x800 ? 2 : 3)

uns ucs2_to_utf8(byte *, word *);
uns utf8_to_ucs2(word *, byte *);
byte *static_ucs2_to_utf8(word *);
uns Ustrlen(word *);
uns utf8_strlen(byte *str);
uns utf8_strnlen(byte *str, uns n);

#endif
