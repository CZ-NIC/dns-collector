/*
 *	Sherlock Library -- Regular Expressions
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *	(c) 2001 Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/chartype.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>

#define INITIAL_MEM 1024		/* Initial space allocated for each pattern */
#define CHAR_SET_SIZE 256		/* How many characters in the character set.  */

struct regex {
  struct re_pattern_buffer buf;
  struct re_registers regs;		/* Must not change between re_match() calls */
  int len_cache;
};

regex *
rx_compile(byte *p, int icase)
{
  regex *r = xmalloc_zero(sizeof(regex));
  const char *msg;

  r->buf.buffer = xmalloc(INITIAL_MEM);
  r->buf.allocated = INITIAL_MEM;
  if (icase)
    {
      unsigned i;
      r->buf.translate = xmalloc (CHAR_SET_SIZE);
      /* Map uppercase characters to corresponding lowercase ones.  */
      for (i = 0; i < CHAR_SET_SIZE; i++)
        r->buf.translate[i] = Cupcase(i);
    }
  else
    r->buf.translate = NULL;
  re_set_syntax(RE_SYNTAX_POSIX_EXTENDED);
  msg = re_compile_pattern(p, strlen(p), &r->buf);
  if (!msg)
    return r;
  die("Error parsing pattern `%s': %s", p, msg);
}

void
rx_free(regex *r)
{
  xfree(r->buf.buffer);
  xfree(r);
}

int
rx_match(regex *r, byte *s)
{
  int len = strlen(s);

  r->len_cache = len;
  if (re_match(&r->buf, s, len, 0, &r->regs) < 0)
    return 0;
  if (r->regs.start[0] || r->regs.end[0] != len) /* XXX: Why regex doesn't enforce implicit "^...$" ? */
    return 0;
  return 1;
}

int
rx_subst(regex *r, byte *by, byte *src, byte *dest, uns destlen)
{
  byte *end = dest + destlen - 1;

  if (!rx_match(r, src))
    return 0;

  while (*by)
    {
      if (*by == '\\')
	{
	  by++;
	  if (*by >= '0' && *by <= '9')	/* \0 gets replaced by entire pattern */
	    {
	      uns j = *by++ - '0';
	      if (j < r->regs.num_regs)
		{
		  byte *s = src + r->regs.start[j];
		  uns i = r->regs.end[j] - r->regs.start[j];
		  if (r->regs.start[j] > r->len_cache || r->regs.end[j] > r->len_cache)
		    return -1;
		  if (dest + i >= end)
		    return -1;
		  memcpy(dest, s, i);
		  dest += i;
		  continue;
		}
	    }
	}
      if (dest < end)
	*dest++ = *by++;
      else
	return -1;
    }
  *dest = 0;
  return 1;
}

#ifdef TEST

void main(int argc, char **argv)
{
  regex *r;
  byte buf1[256], buf2[256];

  r = rx_compile(argv[1]);
  while (fgets(buf1, sizeof(buf1), stdin))
    {
      char *p = strchr(buf1, '\n');
      if (p)
	*p = 0;
      if (argc == 2)
	{
	  if (rx_match(r, buf1))
	    puts("MATCH");
	  else
	    puts("NO MATCH");
	}
      else
	{
	  int i = rx_subst(r, argv[2], buf1, buf2, sizeof(buf2));
	  if (i < 0)
	    puts("OVERFLOW");
	  else if (!i)
	    puts("NO MATCH");
	  else
	    puts(buf2);
	}
    }
  rx_free(r);
}

#endif
