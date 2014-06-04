/*
 *	The UniCode Character Categorizer
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _CHARSET_UNICAT_H
#define _CHARSET_UNICAT_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define Uexpand_lig ucw_Uexpand_lig
#define _U_cat ucw__U_cat
#define _U_lower ucw__U_lower
#define _U_unaccent ucw__U_unaccent
#define _U_upper ucw__U_upper
#endif

extern const byte *_U_cat[];
extern const u16 *_U_upper[], *_U_lower[], *_U_unaccent[];

static inline uint Ucategory(uint x)
{
  if (_U_cat[x >> 8U])
    return _U_cat[x >> 8U][x & 0xff];
  else
    return 0;
}

static inline uint Utoupper(uint x)
{
  uint w = (_U_upper[x >> 8U]) ? _U_upper[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline uint Utolower(uint x)
{
  uint w = (_U_lower[x >> 8U]) ? _U_lower[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

static inline uint Uunaccent(uint x)
{
  uint w = (_U_unaccent[x >> 8U]) ? _U_unaccent[x >> 8U][x & 0xff] : 0;
  return w ? w : x;
}

extern const u16 *Uexpand_lig(uint x);

enum unicode_char_type {
  _U_LETTER = 1,		/* Letters */
  _U_UPPER = 2,			/* Upper-case letters */
  _U_LOWER = 4,			/* Lower-case letters */
  _U_CTRL = 8,			/* Control characters */
  _U_DIGIT = 16,		/* Digits */
  _U_XDIGIT = 32,		/* Hexadecimal digits */
  _U_SPACE = 64,		/* White spaces (spaces, tabs, newlines) */
  _U_LIGATURE = 128,		/* Compatibility ligature (to be expanded) */
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

#endif
