/*
 *	Sherlock Library -- Unicode Characters
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UNICODE_H
#define _UNICODE_H

/* Macros for handling UTF-8 */

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

#define UTF8_SKIP_BWD(p) while ((--*(p) & 0xc0) == 0x80)

#define UTF8_SPACE(u) ((u) < 0x80 ? 1 : (u) < 0x800 ? 2 : 3)

/* unicode-utf8.c */

uns utf8_strlen(byte *str);
uns utf8_strnlen(byte *str, uns n);

#endif
