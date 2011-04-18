/*
 *	UCW Library -- Matching Prefixes and Suffixes
 *
 *	(c) 2011 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/string.h"

#include <string.h>

int
str_has_prefix(char *str, char *prefix)
{
  size_t pxlen = strlen(prefix);
  return !strncmp(str, prefix, pxlen);
}

int
str_has_suffix(char *str, char *suffix)
{
  size_t sxlen = strlen(suffix);
  size_t len = strlen(str);

  if (len < sxlen)
    return 0;
  else
    return !strcmp(str + len - sxlen, suffix);
}

int
str_hier_prefix(char *str, char *prefix, uns sep)
{
  while (*str && *prefix)
    {
      size_t sl=0, pl=0;
      while (str[sl] && (uns) str[sl] != sep)
	sl++;
      while (prefix[pl] && (uns) prefix[pl] != sep)
	pl++;
      if (sl != pl || memcmp(str, prefix, sl))
	return 0;
      str += sl, prefix += pl;
      if (!*str)
	return !*prefix;
      if (!*prefix)
	return 1;
      str++, prefix++;
    }
  return !*prefix;
}

int
str_hier_suffix(char *str, char *suffix, uns sep)
{
  char *st = str + strlen(str);
  char *sx = suffix + strlen(suffix);
  while (st > str && sx > suffix)
    {
      size_t sl=0, pl=0;
      while (st-sl > str && (uns) st[-sl-1] != sep)
	sl++;
      while (sx-pl > suffix && (uns) sx[-pl-1] != sep)
	pl++;
      if (sl != pl || memcmp(st-sl, sx-pl, sl))
	return 0;
      st -= sl, sx -= pl;
      if (st == str)
	return (sx == suffix);
      if (sx == suffix)
	return 1;
      st--, sx--;
    }
  return (sx == suffix);
}

#ifdef TEST

#include <stdio.h>

int main(int argc, char **argv)
{
  if (argc != 4)
    return 1;

  int ret;
  switch (argv[1][0])
    {
    case 'p':
      ret = str_has_prefix(argv[2], argv[3]);
      break;
    case 's':
      ret = str_has_suffix(argv[2], argv[3]);
      break;
    case 'P':
      ret = str_hier_prefix(argv[2], argv[3], '.');
      break;
    case 'S':
      ret = str_hier_suffix(argv[2], argv[3], '.');
      break;
    default:
      return 1;
    }
  printf("%s\n", (ret ? "YES" : "NO"));
  return 0;
}

#endif
