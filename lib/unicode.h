/*
 *	Sherlock Library -- Unicode Characters
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
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

#define PUT_UTF8_32(p,u) do {		\
  if (u < (1<<16))			\
    PUT_UTF8(p,u);			\
  else if (u < (1<<21))			\
    {					\
      *p++ = 0xf0 | (u >> 18);		\
      *p++ = 0x80 | ((u >> 12) & 0x3f);	\
      *p++ = 0x80 | ((u >> 6) & 0x3f);	\
      *p++ = 0x80 | (u & 0x3f);		\
    }					\
  else if (u < (1<<26))			\
    {					\
      *p++ = 0xf8 | (u >> 24);		\
      *p++ = 0x80 | ((u >> 18) & 0x3f);	\
      *p++ = 0x80 | ((u >> 12) & 0x3f);	\
      *p++ = 0x80 | ((u >> 6) & 0x3f);	\
      *p++ = 0x80 | (u & 0x3f);		\
    }					\
  else if (u < (1U<<31))		\
    {					\
      *p++ = 0xfc | (u >> 30);		\
      *p++ = 0x80 | ((u >> 24) & 0x3f);	\
      *p++ = 0x80 | ((u >> 18) & 0x3f);	\
      *p++ = 0x80 | ((u >> 12) & 0x3f);	\
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

#define GET_UTF8_32_CHAR(p,u) do {	\
    if (*p < 0xf0)			\
      GET_UTF8_CHAR(p,u);		\
    else if (*p < 0xf8)			\
      {					\
	u = *p++ & 0x07;		\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
      }					\
    else if (*p < 0xfc)			\
      {					\
	u = *p++ & 0x03;		\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
      }					\
    else if (*p < 0xfe)			\
      {					\
	u = *p++ & 0x01;		\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)       	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
	if ((*p & 0xc0) == 0x80)	\
	  u = (u << 6) | (*p++ & 0x3f);	\
      }					\
    else				\
      {	/* Too large, use replacement char */	\
	p++;				\
	while ((*p & 0xc0) == 0x80)	\
	  p++;				\
	u = UNI_REPLACEMENT;		\
      }					\
  } while (0)				\

#define GET_UTF8(p,u)			\
    if (IS_UTF8(*p))			\
      GET_UTF8_CHAR(p,u);		\
    else				\
      u = *p++

#define GET_UTF8_32(p,u)		\
    if (IS_UTF8(*p))			\
      GET_UTF8_32_CHAR(p,u);		\
    else				\
      u = *p++

#define UTF8_SKIP(p) do {				\
    uns c = *p++;					\
    if (c >= 0xc0)					\
      while (c & 0x40 && *p >= 0x80 && *p < 0xc0)	\
        p++, c <<= 1;					\
  } while (0)

#define UTF8_SKIP_BWD(p) while ((*--(p) & 0xc0) == 0x80)

static inline uns
utf8_space(uns u)
{
  if (u < 0x80)
    return 1;
  if (u < 0x800)
    return 2;
  if (u < (1<<16))
    return 3;
  if (u < (1<<21))
    return 4;
  if (u < (1<<26))
    return 5;
  return 6;
}

static inline uns
utf8_encoding_len(uns c)
{
  if (c < 0x80)
    return 1;
  ASSERT(c >= 0xc0 && c < 0xfe);
  if (c < 0xe0)
    return 2;
  if (c < 0xf0)
    return 3;
  if (c < 0xf8)
    return 4;
  if (c < 0xfc)
    return 5;
  return 6;
}

/* unicode-utf8.c */

uns utf8_strlen(byte *str);
uns utf8_strnlen(byte *str, uns n);

#endif
