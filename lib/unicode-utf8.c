/*
 *	UCW Library -- UTF-8 Functions
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2003 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/unicode.h"

uns
utf8_strlen(byte *str)
{
  uns len = 0;
  while (*str)
    {
      UTF8_SKIP(str);
      len++;
    }
  return len;
}

uns
utf8_strnlen(byte *str, uns n)
{
  uns len = 0;
  byte *end = str + n;
  while (str < end)
    {
      UTF8_SKIP(str);
      len++;
    }
  return len;
}

#ifdef TEST
#include <string.h>
#include <stdio.h>
int main(int argc, char **argv)
{
  byte buf[256];
  if (argc > 1 && !strncmp(argv[1], "get", 3))
    {
      int f32 = !strcmp(argv[1], "get32");
      byte *p = buf;
      uns u;
      while (scanf("%x", &u) == 1)
	*p++ = u;
      *p = 0;
      p = buf;
      while (*p)
	{
	  if (p != buf)
	    putchar(' ');
	  if (f32)
	    GET_UTF8_32(p, u);
	  else
	    GET_UTF8(p, u);
	  printf("%04x", u);
	}
      putchar('\n');
    }
  else if (argc > 1 && !strncmp(argv[1], "put", 3))
    {
      uns u, i=0;
      int f32 = !strcmp(argv[1], "put32");
      while (scanf("%x", &u) == 1)
	{
	  byte *p = buf;
	  if (f32)
	    PUT_UTF8_32(p, u);
	  else
	    PUT_UTF8(p, u);
	  *p = 0;
	  for (p=buf; *p; p++)
	    {
	      if (i++)
		putchar(' ');
	      printf("%02x", *p);
	    }
	}
      putchar('\n');
    }
  else
    puts("?");
  return 0;
}
#endif
