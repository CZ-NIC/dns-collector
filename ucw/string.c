/*
 *	UCW Library -- String Routines
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2007--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "ucw/lib.h"
#include "ucw/string.h"

#include <string.h>

#ifdef CONFIG_DARWIN
uns
strnlen(const char *str, uns n)
{
  const char *end = str + n;
  const char *c;
  for (c = str; *c && c < end; c++);
  return c - str;
}
#endif

char *
str_format_flags(char *dest, const char *fmt, uns flags)
{
  char *start = dest;
  for (uns i=0; fmt[i]; i++)
    {
      if (flags & (1 << i))
	*dest++ = fmt[i];
      else
	*dest++ = '-';
    }
  *dest = 0;
  return start;
}

uns
str_count_char(const char *str, uns chr)
{
  const byte *s = str;
  uns i = 0;
  while (*s)
    if (*s++ == chr)
      i++;
  return i;
}

int
str_starts_with(const char *haystack, const char *needle)
{
  while (*needle)
    if (*haystack++ != *needle++)
      return 0;
  return 1;
}

int
str_ends_with(const char *haystack, const char *needle)
{
  int hlen = strlen(haystack);
  int nlen = strlen(needle);
  if (hlen < nlen)
    return 0;
  else
    return !memcmp(haystack + hlen - nlen, needle, nlen);
}
